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
#include <net/ppp_defs.h>
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

#define IPDEST_UNKNOWN	(0)
#define IPDEST_INTERFACE (1)
#define IPDEST_X (2)
#define IPDEST_BLUE (4)
#define IPDEST_FRAGERR (-1)
#define IPDEST_RUNTERR (-2)
#define IPDEST_FIREWALLERR (-3)
#define IPSRC_INTERFACE IPDEST_INTERFACE
#define IPSRC_X IPDEST_X
#define IPSRC_BLUE IPDEST_BLUE

#define FIREWALL_ACCEPTED (0)

/*
 * IP Fragment handling:
 * Maintain a tailq of fraghead structures, each of which
 *  heads a list of mbuf chains which are the fragments
 *  of an IP datagram.
 * The 'fraghead' includes an offset indicator, to be used as a
 *  possible optimization to avoid timeouts on a completed fragment
 *  list.
 */
struct fraghead {
    TAILQ_ENTRY(fraghead) fh_link;
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

/* In support of ipfw (firewall) */
extern u_int16_t ip_divert_cookie;

extern int hz;		/* in clock.c (yuck) */

int check_icmp(struct icmp *);
int not4us(struct ip *, struct ifnet *);
int validate_addrs(struct BlueFilter *, struct blueCtlBlock *);
void sip_fragtimer(void *);
int find_ip_dest(struct mbuf**, int, struct blueCtlBlock*, struct ifnet**);
int lookup_frag(struct ip*, struct blueCtlBlock *ifb);
void handle_first_frag(struct ip* ip, int inOwner, struct blueCtlBlock *ifb);
void hold_frag(struct mbuf* m, int headerSize, struct blueCtlBlock *ifb);
int process_firewall_in(struct mbuf** m_orig);
int process_firewall_out(struct mbuf** m_orig, struct ifnet* ifp);
int ipv4_detach(caddr_t  cookie);
extern struct if_proto *dlttoproto(u_long);

/*
 * Get user's filtering info; prep for state check in caller.  We
 *  can modify state by registering for new ownership id's.
 * Requested addresses are validated against those registered for
 *  the requested interface.
 */
int
enable_ipv4(struct BlueFilter *bf, void *data, struct blueCtlBlock *ifb)
{
    int retval;
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
            &ifb->tcp_blue_owned)) != 0) {
            if (in_pcb_rem_share_client(&tcbinfo,
                ifb->udp_blue_owned) == 0)
                ifb->udp_blue_owned = 0;
            return (-retval);
        }
    } else
        return(-EOPNOTSUPP);
    ifp = ((struct ndrv_cb *)ifb->ifb_so->so_pcb)->nd_if;
    ifa = ifp->if_addrhead.tqh_first;
    while (ifa) {
        if ((sd = (struct sockaddr_dl *)ifa->ifa_addr)
                && sd->sdl_family == AF_LINK) {
            register unsigned char *p;
            if (sd->sdl_type == IFT_ETHER) {
                MALLOC(p, char *, 6, M_TEMP, M_WAITOK);
                if (p == NULL)
                    return(-ENOMEM);
                bcopy(&sd->sdl_data[sd->sdl_nlen], p, 6);
                ifb->media_addr_size = 6;
                ifb->dev_media_addr = p;
                break;
            }
            if (sd->sdl_type == IFT_PPP) {
                ifb->media_addr_size = 0;
                /* No media address */
                ifb->dev_media_addr = NULL;
                break;
            }
        }
        ifa = ifa->ifa_link.tqe_next;
    }
    
    if (ifa) {
        int s;
        s = splnet();
        if ((retval = ipv4_attach_protofltr(ifp, ifb)) != 0) {
#if SIP_DEBUG_ERR
            log(LOG_WARNING,
                "enable_ipv4: failed, ifb=%d retval=%d\n",
                ifb, retval);
#endif
            splx(s);
            return(-retval);
        }
        splx(s);
        return(BFS_IP);
    } else {
        if (in_pcb_rem_share_client(&tcbinfo,
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
 *  X, blue, or both.
 * Note: the packet is "fully formed", i.e., has a media header.
 * NB: Assumes the X stack is operational
 */
int  ipv4_ppp_infltr(caddr_t cookie,
		 struct mbuf   **m_orig,
		 char          **pppheader,
		 struct ifnet  **ifnet_ptr)
{
    struct blueCtlBlock *ifb = (struct blueCtlBlock *)cookie;
    
    /* Only do work if we're filtering for packets. */
    if (ifb->filter[BFS_IP].BF_flags) {
        
        /* Get the destination of the IP packet. */
        int destination = find_ip_dest(m_orig, IPSRC_INTERFACE, ifb, ifnet_ptr);
        if (destination < 0) {
            /* Handle the errror */
            if (destination == IPDEST_FRAGERR) {
                /*
                 * This is a fragment of a larger IP packet, it is not the
                 * first packet so we can not determine the destination.
                 */ 
                hold_frag(*m_orig, 0, ifb);
                *m_orig = NULL;
            } else if (*m_orig != NULL)
                /* For whatever reason, we couldn't determine the dest, send to X. */
                destination = IPDEST_X;
        }
        if (destination == IPDEST_UNKNOWN)
            destination = IPDEST_X | IPDEST_BLUE;
        
        /*
         * Based on destination, either duplicate the packet for blue,
         * send it to blue, or leave it alone, the caller will pass it to X.
         */
        if (destination > 0) {
            struct mbuf* m0 = NULL;
            
            if (destination == IPDEST_BLUE) {
                m0 = *m_orig;
                *m_orig = NULL;
            } else if ((destination & IPDEST_BLUE) && *m_orig != NULL)
                m0 = m_dup(*m_orig, M_NOWAIT);
            
            /* Send m0 to client (blue). */
            if (m0 && process_firewall_in(&m0) == FIREWALL_ACCEPTED && m0)
                blue_inject(ifb, m0);
        }
    }
    
    /* 
     * If we haven't sent the packet elsewhere, return 0. If we have,
     * return EJUSTRETURN.
     */
    return *m_orig != NULL ? 0 : EJUSTRETURN;
}

int
ipv4_ppp_outfltr(caddr_t cookie,
	     struct mbuf   **m_orig,
	     struct ifnet  **ifnet_ptr,
	     struct sockaddr **dest,
	     char *dest_linkaddr,
	     char *frame_type)
{
    struct blueCtlBlock *ifb = (struct blueCtlBlock *)cookie;
    
    /* Check if filtering is enabled. */
    if (ifb->filter[BFS_IP].BF_flags) {
        struct mbuf* m0 = NULL;
        
        /* Find the destination for this IP packet */
        int destination = find_ip_dest(m_orig, IPSRC_X, ifb, ifnet_ptr);
        
        if (destination < 0) {
            if (destination == IPDEST_FRAGERR) {
                /*
                 * We assume we will not get fragments out of order from the X
                 * stack. This assumption has been tested. It seems we don't
                 * even get fragments.
                 * If this assumption breaks, handle_first_frag needs to change.
                 */
#if SIP_DEBUG_ERR
                log(LOG_WARNING, "ipv4_ppp_outfltr: received fragments out of order from X statck!");
#endif
            }
            /* In the case of an error, default to sending on the interface. */
            if (*m_orig != NULL)
                destination = IPDEST_INTERFACE;
        }
        
        /* Default to the interface. */
        if (destination == IPDEST_UNKNOWN)
            destination = IPDEST_INTERFACE;
        
        /*
         * Based on destination, either send the packet to blue, dupe
         * the packet so the dupe may be sent to blue, or leave the
         * packet alone, it will be sent to the interface.
         */
        if (destination > 0) {
            if (destination == IPDEST_BLUE) {
                m0 = *m_orig;
                *m_orig = NULL;
            } else if ((destination & IPDEST_BLUE) && *m_orig != NULL)
                m0 = m_dup(*m_orig, M_NOWAIT);
            if (m0 != NULL)
                blue_inject(ifb, m0);
        }
    }
    
    /* If we didn't consume the packet (send it to blue), return 0 */
    return *m_orig != NULL ? 0 : EJUSTRETURN;
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
{
    struct blueCtlBlock *ifb = (struct blueCtlBlock *)cookie;
    
    if (ifb->filter[BFS_IP].BF_flags) {
        struct ether_header* header = (struct ether_header *)*etherheader;
        struct mbuf* m0 = NULL;
        int destination = IPDEST_UNKNOWN;
        
        if(header->ether_type == ETHERTYPE_ARP) {
            struct arphdr* ah = NULL;
            
            /* Get the ARP header, only ethernet arp replies should go to both. */
            if (do_pullup(m_orig, sizeof(struct arphdr)) == 0) {
                ah = mtod(*m_orig, struct arphdr*);
                if ((ah->ar_pro == ETHERTYPE_IP) &&
                    (ah->ar_op == ARPOP_REPLY))
                    destination = IPDEST_X | IPDEST_BLUE;
                else
                    destination = IPDEST_X;
            } else
                destination = IPDEST_RUNTERR;
        } else if (header->ether_type == ETHERTYPE_IP) {
            if (**etherheader & 0x01) /* Destination is multicast/broadcast */
                destination = IPDEST_X | IPDEST_BLUE;
            else
                destination = find_ip_dest(m_orig, IPSRC_INTERFACE, ifb, ifnet_ptr);
        } else
            /* If it isn't IP or arp, send it to both. */
            destination = IPDEST_X | IPDEST_BLUE;
        
        /*
         * Include the ethernet header before we send the packet to blue,
         * dupe it to be sent to blue, or hold it to be sent later.
         */
         if (*m_orig != NULL)
            MDATA_INCLUDE_HEADER(*m_orig, sizeof(struct ether_header));
        if (destination < 0) {
            if (destination == IPDEST_FRAGERR) {
                /*
                 * Fragment of packet, couldn't determine destination.
                 * Hold on to packet so when the destination can be
                 * determined, we can send it to the right place.
                 */
                hold_frag(*m_orig, sizeof(struct ether_header), ifb);
                *m_orig = NULL;
            } else if (*m_orig != NULL)
                destination = IPDEST_X;
        } else if (destination == IPDEST_UNKNOWN)
            destination = IPDEST_X | IPDEST_BLUE;
        if (destination > 0) {
            if (destination == IPDEST_BLUE) {
                m0 = *m_orig;
                *m_orig = NULL;
            } else if (destination & IPDEST_BLUE)
                m0 = m_dup(*m_orig, M_NOWAIT);
            /* let the firewall take a crack at stuff for classic */
            if (m0 && header->ether_type == ETHERTYPE_IP) {
                MDATA_REMOVE_HEADER(m0, sizeof(struct ether_header));
                if(process_firewall_in(&m0) == FIREWALL_ACCEPTED)
                    MDATA_INCLUDE_HEADER(m0, sizeof(struct ether_header));
            }
            if (m0)
                blue_inject(ifb, m0);
        }
        
        /* Remove the ethernet header we included above before returning. */
        if (*m_orig != NULL)
            MDATA_REMOVE_HEADER(*m_orig, sizeof(struct ether_header));
    }
    /* If we didn't consume the packet (send it to blue), return 0 */
    return *m_orig != NULL ? 0 : EJUSTRETURN;
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
{
    struct blueCtlBlock* ifb = (struct blueCtlBlock*)cookie;
    
    if (ifb->filter[BFS_IP].BF_flags) {
        struct mbuf* m0 = NULL;
        int destination = IPDEST_UNKNOWN;
        if ((*dest)->sa_family == AF_INET) {
            /* Find the destination for the IP packet */
            destination = find_ip_dest(m_orig, IPSRC_X, ifb, ifnet_ptr);
        }
        if (destination < 0) {
            if (destination == IPDEST_FRAGERR) {
                /* We don't handle out of order fragments from the X stack. */
#if SIP_DEBUG
                log(LOG_WARNING, "SIP - ipv4_eth_outfltr: received IPDEST_FRAGERR!!!");
#endif
                destination = IPDEST_INTERFACE;
            } else if (*m_orig != NULL)
                destination = IPDEST_INTERFACE;
        }
        if (destination > 0) {
            if (destination == IPDEST_BLUE) {
                m0 = *m_orig;
                *m_orig = NULL;
            } else if ((destination & IPDEST_BLUE) && *m_orig != NULL)
                m0 = m_dup(*m_orig, M_NOWAIT);
            if (m0) {
                struct ether_header *eh;
                
                /* Fake an ethernet header for the packet before we send it to blue. */
                M_PREPEND(m0, sizeof(struct ether_header), M_NOWAIT);
                if (m0 != NULL) {
                    eh = mtod(m0, struct ether_header*);
                    eh->ether_type = *(unsigned short*)frame_type;
                    bcopy(ifb->dev_media_addr, m0->m_data, ifb->media_addr_size);
                    blue_inject(ifb, m0);
                }
            }
        }
    }
    /* If we didn't consume the packet (send it to blue), return 0 */
    return *m_orig != NULL ? 0 : EJUSTRETURN;
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
{
    struct blueCtlBlock* ifb = (struct blueCtlBlock*)cookie;
    
    if (ifb->filter[BFS_IP].BF_flags) {
        int destination = IPDEST_UNKNOWN;
        struct mbuf* m0 = NULL;
        
        if ((*dest)->sa_family == AF_INET) {
            destination = find_ip_dest(m_orig, IPSRC_X, ifb, ifnet_ptr);
            if (destination < 0 && *m_orig != NULL)
                destination = IPDEST_INTERFACE;
        }
        if (destination == IPDEST_UNKNOWN)
            destination = IPDEST_INTERFACE;
        if (destination > 0) {
            if (destination == IPDEST_BLUE) {
                m0 = *m_orig;
                *m_orig = NULL;
            } else if ((destination & IPDEST_BLUE) && *m_orig != NULL)
                m0 = m_dup(*m_orig, M_NOWAIT);
            if (m0 != NULL) {
                /*
                 * Send m0 to blue. We may have to prepend an ethernet header
                 * if blue is expecting it.
                 */
                if (ifb->media_addr_size == 6) {
                    struct ether_header *eh;
                    M_PREPEND(m0, sizeof(struct ether_header), M_NOWAIT);
                    if (m0 != NULL)
                    {
                        eh = mtod(m0, struct ether_header*);
                        eh->ether_type = *(unsigned short*)frame_type;
                        bcopy(ifb->dev_media_addr, m0->m_data, ifb->media_addr_size);
                    }
                }
                if (m0)
                    blue_inject(ifb, m0);
            }
        }
    }
    
    return *m_orig != NULL ? 0 : EJUSTRETURN;
}

/*
 * We get a packet destined for the outside world, complete with ethernet
 *  header.  If it goes to X, we need to get rid of the header.
 *
 * This function is much like the others before it. If you're curious about
 * what's going on here, it's commented in the functions above.
 */
int
si_send_eth_ipv4(register struct mbuf **m_orig, struct blueCtlBlock *ifb,
    struct ifnet *ifp)
{    
    if (ifb->filter[BFS_IP].BF_flags & SIP_PROTO_RCV_SHARED) {
        struct ether_header* enetHeader = NULL;
        int destination = IPDEST_UNKNOWN;
        struct mbuf* m1 = NULL;
        
        if (do_pullup(m_orig, sizeof(struct ether_header)) == 0) {
            enetHeader = mtod(*m_orig, struct ether_header*);
            if (enetHeader->ether_type == ETHERTYPE_ARP)
                destination = IPDEST_INTERFACE;
            else if (enetHeader->ether_type == ETHERTYPE_IP) {
                MDATA_REMOVE_HEADER(*m_orig, sizeof(struct ether_header));
                destination = find_ip_dest(m_orig, IPSRC_BLUE, ifb,
                                &((struct ndrv_cb *)ifb->ifb_so->so_pcb)->nd_if);
                if (*m_orig != 0) {
                    /* Let the firewall have a crack at it. */
                    if (process_firewall_out(m_orig, ifp) != 0)
                        destination = IPDEST_FIREWALLERR;
                    else
                MDATA_INCLUDE_HEADER(*m_orig, sizeof(struct ether_header));
                }
            } else
                destination = IPDEST_UNKNOWN;
        } else
            destination = IPDEST_RUNTERR;
        if ((destination < 0 && *m_orig != NULL) || destination == IPDEST_UNKNOWN)
            destination = IPDEST_INTERFACE;
        if (destination > 0) {
            if (destination == IPDEST_X) {
                m1 = *m_orig;
                *m_orig = NULL;
            } else if ((destination & IPDEST_X) && *m_orig != NULL)
                m1 = m_dup(*m_orig, M_NOWAIT);
            if (m1 != NULL) {
                char* pHeader = mtod(m1, char*);
                MDATA_REMOVE_HEADER(m1, sizeof(struct ether_header));
                m1->m_pkthdr.rcvif = ((struct ndrv_cb*)ifb->ifb_so->so_pcb)->nd_if;
                dlil_inject_pr_input(m1, pHeader, ifb->ipv4_proto_filter_id);
            }
        }
    }
    return *m_orig != NULL ? 0 : EJUSTRETURN;
}

/*
 * We get a packet destined for the outside world, complete with PPP
 *  'header'. The mbuf's m_data member points to the IP header, the PPP
 *  'header' is located in front of that, but since blue doesn't want
 *  it, nor does our find_ip_dest, we just ignore it.
 *
 * This function is much like the others before it. If you're curious about
 * what's going on here, it's commented in the functions above
 */
int
si_send_ppp_ipv4(register struct mbuf **m_orig, struct blueCtlBlock *ifb,
                 struct ifnet *ifp)
{
    if (ifb->filter[BFS_IP].BF_flags & SIP_PROTO_RCV_SHARED) {
        int destination = IPDEST_UNKNOWN;
        struct mbuf* m1 = NULL;
                
        destination = find_ip_dest(m_orig, IPSRC_BLUE, ifb,
                        &((struct ndrv_cb *)ifb->ifb_so->so_pcb)->nd_if);
        if (*m_orig != NULL) {
            if(process_firewall_out(m_orig, ifp) != FIREWALL_ACCEPTED)
                destination = IPDEST_FIREWALLERR;
        }
        if ((destination < 0 && *m_orig != NULL) || destination == IPDEST_UNKNOWN)
            destination = IPDEST_INTERFACE;
        if (destination == IPDEST_X) {
            m1 = *m_orig;
            *m_orig = NULL;
        } else if ((destination & IPDEST_X) && *m_orig != NULL)
            m1 = m_dup(*m_orig, M_NOWAIT);
        if (m1 != NULL) {
            char* pHeader = mtod(m1, char*);
            m1->m_pkthdr.rcvif = ((struct ndrv_cb*)ifb->ifb_so->so_pcb)->nd_if;
            dlil_inject_pr_input(m1, pHeader, ifb->ipv4_proto_filter_id);
        }
    }
    return *m_orig != NULL ? 0 : EJUSTRETURN;
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
    struct dlil_pr_flt_str filter1 = {
        (caddr_t)ifb,
        0, 0,
        0,0,ipv4_detach
    };
    struct dlil_pr_flt_str lo_filter = {
        (caddr_t)ifb,
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
                                    PF_INET, &ip4_dltag)) == 0) {
        if (ifp->if_type == IFT_ETHER) {
            filter1.filter_dl_input = ipv4_eth_infltr;
            filter1.filter_dl_output = ipv4_eth_outfltr;
        } else if (ifp->if_type == IFT_PPP) {
            filter1.filter_dl_input = ipv4_ppp_infltr;
            filter1.filter_dl_output = ipv4_ppp_outfltr;
        } /* else??? */
        retval= dlil_attach_protocol_filter(ip4_dltag, &filter1,
                                            &ifb->ipv4_proto_filter_id,
                                            DLIL_LAST_FILTER);
        if (retval == 0) {
            retval = dlil_find_dltag(loif[0].if_family,
                                        loif[0].if_unit,
                                        PF_INET, &lo_dltag);
            if (retval == 0)
                retval = dlil_attach_protocol_filter(lo_dltag,
                                                    &lo_filter,
                                                    &ifb->lo_proto_filter_id,
                                                    DLIL_LAST_FILTER);
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
{
    struct fraghead *frag;
    struct mbuf *m, *m1;
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
    if (ifb->ipv4_proto_filter_id) {
#if SIP_DEBUG
        log(LOG_WARNING,
            "ipv4_stop: deregister IPv4 proto filter tag=%d\n",
            ifb->ipv4_proto_filter_id);
#endif
        retval = dlil_detach_filter(ifb->ipv4_proto_filter_id);
        if (retval) {
#if SIP_DEBUG_ERR
            log(LOG_WARNING, "STOP/FILTER1: %d\n", retval);
#endif
        }
    }
    
    if (ifb->lo_proto_filter_id) {
        retval = dlil_detach_filter(ifb->lo_proto_filter_id);
        if (retval) {
#if SIP_DEBUG_ERR
            log(LOG_WARNING, "STOP/FILTER2: %d\n", retval);
#endif
        }
        else
            ifb->lo_proto_filter_id = 0;
    }
    
    if (retval == 0 && ifb->fraglist_timer_on) {
        ifb->ClosePending = 1;
        retval = EBUSY;	/* Assume retval isn't changed from here down */
        untimeout(sip_fragtimer, ifb);
    } else
        ifb->ClosePending = 0; /* In case... */
    /* Clean out frag list */
    while ((frag = TAILQ_FIRST(&ifb->fraglist)) != 0) {
        TAILQ_REMOVE(&ifb->fraglist, frag, fh_link);
        m = TAILQ_FIRST(&(frag->fh_frags));
        while (m) {
            m1 = m->m_nextpkt;
            m_freem(m); /* Free a chain */
            m = m1;
        }
        FREE(frag, M_TEMP);
    }
    
    /* Fall through */
error:
    ifb->ipv4_stopping = 0;
    return retval;
}

int
ipv4_control(struct socket *so, struct sopt_shared_port_param *psp,
	     struct kextcb *kp, int cmd)
{
    int retval;
    struct inpcbinfo *pcbinfo;
    struct blueCtlBlock *ifb = (struct blueCtlBlock *)kp->e_fcb;
    u_char owner = 0;
    u_short lport;
    switch(cmd) {
        case SO_PORT_RESERVE: {
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
        case SO_PORT_RELEASE: {
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
            psp->faddr.s_addr = 0;
            psp->fport 	  = 0;
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
        case SO_PORT_LOOKUP: {
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
{
    struct ifnet *ifp;
    struct ifaddr *ifa;
    struct sockaddr_in *sin;

    ifp = ((struct ndrv_cb *)ifb->ifb_so->so_pcb)->nd_if;
    ifa = ifp->if_addrhead.tqh_first;
    while (ifa) {
        if ((sin = (struct sockaddr_in *)ifa->ifa_addr)
            && sin->sin_family == AF_INET)
            if (sin->sin_addr.s_addr == bf->BF_address)
                break;

        ifa = ifa->ifa_link.tqe_next;
    }
    if (ifa == 0) {
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
{
    struct ifaddr *ifa;
    struct sockaddr_in *sin;

    ifa = ifp->if_addrhead.tqh_first;
    while (ifa) {
        if ((sin = (struct sockaddr_in *)ifa->ifa_addr)
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
 * IP Frag queue processing:
 *  if a timer expires on a frag list, discard it.
 * Start this when we enter the first frag header into the list
 * Restart after every sweep.
 * Don't restart if list is empty (will not cancel, since races make
 *  it problematic).
 */
void
sip_fragtimer(void *arg)
{
    int s;
    /* Will use this once the frag list is made non-global */
    struct blueCtlBlock *ifb = (struct blueCtlBlock *)arg;
    struct fraghead *frag, *frag1;
    int i = 0;
    boolean_t funnel_state;

    funnel_state = thread_funnel_set(network_flock, TRUE);
    s = splnet();
    if (ifb->ClosePending) {
        release_ifb(ifb);
        splx(s);
        (void) thread_funnel_set(network_flock, funnel_state);
        return;
    }
    frag1 = TAILQ_FIRST(&ifb->fraglist);
    while ((frag = frag1) != 0) {
        frag1 = TAILQ_NEXT(frag, fh_link);
        if (--frag->fh_ttl == 0) {
            struct mbuf *m, *m1;
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

/*
 * This function will return bits set for each destination the packet
 * should go to. If there is an error, the result will be < 0.
 *
 * The mbuf passed in must contain an IP packet, and the m_data
 * must point to the IP header. If there is an ethernet header or
 * ppp header, use MDATA_REMOVE_HEADER to temporarily remove it.
 */
int
find_ip_dest(struct mbuf** m_orig, int inSource,
             struct blueCtlBlock *ifb, struct ifnet **ifnet_ptr)
{
    struct ip *ipHeader = NULL;
    int ipHeaderLength = 0;
    int returnValue = IPDEST_UNKNOWN;
    unsigned short ipOffset;
    
    /* Verify we have a contiguous IP header. */
    if (do_pullup(m_orig, sizeof(struct ip)) != 0)
        return IPDEST_RUNTERR; /* Oops, we just lost the packet */
    ipHeader = mtod(*m_orig, struct ip*);
    if (inSource != IPSRC_INTERFACE) {
        if (not4us(ipHeader, *ifnet_ptr))
            return IPDEST_INTERFACE;
        if (IN_CLASSD(ipHeader->ip_dst.s_addr))
            return (IPDEST_X | IPDEST_BLUE | IPDEST_INTERFACE) & (~inSource);
    }
    
    /*
     * If this is a fragment, and not the first, we won't be able to
     * determine the destination. Lookup the fragment in our list.
     */
    ipOffset = ntohs(ipHeader->ip_off);
    if ((ipOffset & (IP_MF | IP_OFFMASK | IP_RF)) &&
        (ipOffset & IP_OFFMASK) != 0) {
        return lookup_frag(ipHeader, ifb);
    }
    ipHeaderLength = ipHeader->ip_hl << 2;
    switch(ipHeader->ip_p) {
        case IPPROTO_TCP: {
            struct tcphdr* tcpHeader;
            unsigned int lcookie;
            unsigned char owner;
            
            /* Make sure we have a contiguous tcp header. */
            if (do_pullup(m_orig, sizeof(struct tcphdr) + ipHeaderLength) != 0) {
                returnValue = IPDEST_RUNTERR;
                break;
            }
            
            /* Get ptrs to the TCP and IP headers. The IP header may have moved. */
            ipHeader = mtod(*m_orig, struct ip*);
            tcpHeader = mtodAtOffset(*m_orig, ipHeaderLength, struct tcphdr*);
            
            /*
             * Lookup the owner of the addr.port->addr.port in the list to
             * determine if X, Blue, both, or the interface should get this
             */
            owner = in_pcb_get_owner(&tcbinfo,
                                      ipHeader->ip_dst,
                                      tcpHeader->th_dport,
                                      ipHeader->ip_src,
                                      tcpHeader->th_sport,
                                      &lcookie);
            if (owner == ifb->tcp_blue_owned)
                returnValue = IPDEST_BLUE;
            else if (owner == INPCB_NO_OWNER) {
#if SIP_DEBUG
                if (inSource == IPSRC_INTERFACE)
                    log(LOG_WARNING, "SIP: not sure who owns %8.8X:%d->%8.8X:%d",
                            ipHeader->ip_src, tcpHeader->th_sport,
                            ipHeader->ip_dst, tcpHeader->th_dport);
#endif
                returnValue = IPDEST_UNKNOWN;
            } else
                returnValue = IPDEST_X;
        }
        break;
        case IPPROTO_UDP: {
            struct udphdr* udpHeader;
            unsigned int lcookie;
            unsigned char owner;
            
            /* Make sure we have a contiguous udp header. */
            if (do_pullup(m_orig, sizeof(struct udphdr) + ipHeaderLength) != 0) {
                returnValue = IPDEST_RUNTERR;
                break;
            }
            
            /* Get a ptr to the udp and ip headers, the ip header may have moved. */
            ipHeader = mtod(*m_orig, struct ip*);
            udpHeader = mtodAtOffset(*m_orig, ipHeaderLength, struct udphdr*);
            
            /*
             * Lookup the owner of the addr.port->addr.port in the list to
             * determine if X, Blue, both, or the interface should get this
             */
            owner = in_pcb_get_owner(&udbinfo,
                                      ipHeader->ip_dst,
                                      udpHeader->uh_dport,
                                      ipHeader->ip_src,
                                      udpHeader->uh_sport,
                                      &lcookie);
            if (owner == ifb->udp_blue_owned)
                returnValue = IPDEST_BLUE;
            else if (owner & ifb->udp_blue_owned)
                returnValue = IPDEST_BLUE | IPDEST_X;
            else if (owner != INPCB_NO_OWNER)
                returnValue = IPDEST_X;
            else
                returnValue = IPDEST_UNKNOWN;
        }
        break;
        case IPPROTO_ICMP: {
            struct icmp* icmpHeader;
            
            /* Make sure we have at least the ICMP type. */
            if (do_pullup(m_orig, sizeof(u_char) + ipHeaderLength) != 0) {
                returnValue = IPDEST_RUNTERR;
                break;
            }
            
            /* Get a ptr to the ip and icmp headers, the ip header may have moved. */
            ipHeader = mtod(*m_orig, struct ip*);
            icmpHeader = mtodAtOffset(*m_orig, ipHeaderLength, struct icmp*);
            
            /*
             * A few special ICMP types are only allowed to go to X or the
             * interface. The rest of the ICMP packets will go to blue & X
             */
            switch (icmpHeader->icmp_type) {
                case ICMP_ECHO:
                case ICMP_IREQ:
                case ICMP_MASKREQ:
                case ICMP_TSTAMP:
                    returnValue = IPDEST_X | IPDEST_INTERFACE;
                break;
                default: {
                    if (inSource != IPSRC_INTERFACE)
                    {
                        /*
                         * Assume any packets with matching destination and source
                         * IP addresses are intended to stay local (blue<->X).
                         *
                         * This may be a very bad thing.
                         */
                        if(ipHeader->ip_dst.s_addr == ipHeader->ip_src.s_addr)
                            returnValue = IPDEST_X | IPDEST_BLUE;
                        else
                            returnValue = IPDEST_INTERFACE;
                    } else
                        returnValue = IPDEST_BLUE | IPDEST_X;
                }
                break;
            }
        }
        break;
        default:
            returnValue = IPDEST_BLUE | IPDEST_X | IPDEST_INTERFACE;
        break;
    }
    if (returnValue == IPDEST_UNKNOWN)
        returnValue = IPDEST_INTERFACE | IPDEST_X;
    
    /*
     * In the case of the first fragment, we want to record where
     * we sent it so we can send other fragments of the same packet
     * to the same location.
     */
    if ((ipHeader->ip_off & (IP_MF | IP_OFFMASK | IP_RF)) != 0)
        handle_first_frag(ipHeader, returnValue, ifb);
    return returnValue;
}

/*
 * Looks up the packet described in ip to determine if we
 * know the destination of this fragmented packet already.
 *
 * Returns the destination or IPDEST_FRAGERR if we don't
 * know.
 */
int lookup_frag(struct ip* ip, struct blueCtlBlock *ifb)
{
    struct fraghead *frag;
    struct in_addr laddr;
    struct in_addr faddr;
    int	found = 0;
    
    laddr = ip->ip_dst;
    faddr = ip->ip_src;
    TAILQ_FOREACH(frag, &ifb->fraglist, fh_link)
        if (frag->fh_id == ip->ip_id &&
            frag->fh_proto == ip->ip_p &&
            frag->fh_laddr.s_addr == laddr.s_addr &&
            frag->fh_faddr.s_addr == faddr.s_addr) {
            found++;
            break;
        }
    if (!found)
        return IPDEST_FRAGERR;
    return frag->fh_owner;
}

/*
 * Called when the first packet of a fragment is
 * encountered, after the owner has been established.
 * This will add an entry in the list or send any
 * fragments related to the packet waiting and update an entry.
 */
void handle_first_frag(struct ip* ip, int inOwner, struct blueCtlBlock *ifb)
{
    struct fraghead *frag;
    struct in_addr laddr;
    struct in_addr faddr;
    int	found = 0;
    
    /* Try to find an entry for the packet this fragment belongs to. */
    laddr = ip->ip_dst;
    faddr = ip->ip_src;
    TAILQ_FOREACH(frag, &ifb->fraglist, fh_link)
        if (frag->fh_id == ip->ip_id &&
            frag->fh_proto == ip->ip_p &&
            frag->fh_laddr.s_addr == laddr.s_addr &&
            frag->fh_faddr.s_addr == faddr.s_addr) {
            found++;
            break;
        }
    
    if (!found) {
        /* We didn't find one, allocate one and fill it out. */
        MALLOC(frag, struct fraghead*, sizeof(struct fraghead),
                M_TEMP, M_NOWAIT);
        if (frag == NULL) {
#if SIP_DEBUG
            log(LOG_WARNING, "SharedIP - handle_first_frag: can't allocate a new fragheader!");
#endif
            return;
        }
        bzero((char*)frag, sizeof(*frag));
        frag->fh_ttl = 120; /* 60 seconds */
        frag->fh_proto = ip->ip_p;
        frag->fh_laddr = laddr;
        frag->fh_faddr = faddr;
        frag->fh_owner = inOwner;
        frag->fh_id = ip->ip_id;
        TAILQ_INIT(&frag->fh_frags);
        
        /* Start the timer to remove this entry. */
        if (ifb->fraglist_timer_on == 0) {
            ifb->fraglist_timer_on = 1;
            timeout(sip_fragtimer, ifb, hz/2);
        }
        
        /* Add it to the fragment list. */
        TAILQ_INSERT_TAIL(&ifb->fraglist, frag, fh_link);
    } else {
        /*
         * We have already received some fragments of this packet. We
         * need to send those fragments to X, Blue, or the wire. We
         * don't support sending those fragments to the wire yet.
         * This is alright because fragments from X and Blue should
         * not be delivered to us out of order. That's just silly.
         */
        struct mbuf* m1 = NULL;
        struct mbuf* m0 = NULL;
        struct mbuf* m2 = NULL;
        struct mbuf* m = NULL;
        struct mbuf* mNext = NULL;
        
        /*
         * Iterate through the packets, sending them to the correct dest.
         */
        if ( inOwner <= 0 )
        {
            inOwner = IPDEST_X;
        }
        frag->fh_owner = inOwner;
        m = frag->fh_frags.tqh_first;
        
        /* reset the list of fragments */
        TAILQ_INIT(&frag->fh_frags);
        while (m != NULL) {
            char* xDataP = NULL;
            mNext = m->m_nextpkt;
            m->m_nextpkt = NULL;
            
            /* Default to X if we got some sort of error */
            if ((inOwner < 0) || (inOwner == IPDEST_UNKNOWN)) {
                inOwner = IPDEST_X;
            }
            
            /* Duplicate the packet as necessary to send to all destinations. */
            if ((inOwner & IPDEST_BLUE) != 0)
                m0 = m;
            if ((inOwner & IPDEST_INTERFACE) != 0) {
                if (m0 == m)
                    m2 = m_dup(m, M_NOWAIT);
                else
                    m2 = m;
            }
            if ((inOwner & IPDEST_X) != 0) {
                if (m0 == m || m2 == m)
                    m1 = m_dup(m, M_NOWAIT);
                else
                    m1 = m;
                xDataP = mtod(m1, char*);
                m_adj(m1, frag->fh_fsize);
            }
            /* Let the firewall have a crack at packets destined to Classic. */
            if (m0) {
                MDATA_REMOVE_HEADER(m0, frag->fh_fsize);
                if(process_firewall_in(&m0) == 0)
                    MDATA_INCLUDE_HEADER(m0, frag->fh_fsize);
            }
            if (m0)
                blue_inject(ifb, m0);
            if (m1)
                dlil_inject_pr_input(m1, xDataP, ifb->ipv4_proto_filter_id);
            if ( m2	) {
#if SIP_DEBUG_ERR
                log(LOG_WARNING, "SharedIP: handle_first_frag, can't send fragment to the line!");
#endif
                m_freem(m2);
            }
            m = mNext;
        }
    }
}

/*
 * This function takes ownership of an mbuf to be sent later when
 * we know the destination of the fragment.
 *
 * The fragment must contain a header for ethernet, no header for PPP.
 * The headerSize must match the size of the header. This is so we can
 * strip off the header before sending to the X stack, but we need the
 * header in case we need to send it to Blue, which likes headers.
 */
void hold_frag(struct mbuf* m, int headerSize, struct blueCtlBlock *ifb)
{
    struct fraghead *frag;
    struct in_addr laddr;
    struct in_addr faddr;
    int	found = 0;
    struct ip* ip;
    
    if (do_pullup(&m, sizeof(struct ip) + headerSize) != 0) {
        if (m != NULL)
            m_freem(m);
        return;
    }
    ip = mtodAtOffset(m, headerSize, struct ip*);
    laddr = ip->ip_dst;
    faddr = ip->ip_src;
    TAILQ_FOREACH(frag, &ifb->fraglist, fh_link)
        if (frag->fh_id == ip->ip_id &&
            frag->fh_proto == ip->ip_p &&
            frag->fh_laddr.s_addr == laddr.s_addr &&
            frag->fh_faddr.s_addr == faddr.s_addr) {
            found++;
            break;
        }
    if (!found) {
        MALLOC(frag, struct fraghead*, sizeof(struct fraghead),
                M_TEMP, M_NOWAIT);
        if (frag == NULL) {
            m_freem(m);
#if SIP_DEBUG_ERR
            log(LOG_WARNING, "SIP - hold_frag: no memory to add frag record!");
#endif
            return;
        }
        bzero((char*)frag, sizeof(*frag));
        frag->fh_ttl = 120; /* 60 seconds */
        frag->fh_id = ip->ip_id;
        frag->fh_proto = ip->ip_p;
        frag->fh_laddr.s_addr = laddr.s_addr;
        frag->fh_faddr.s_addr = faddr.s_addr;
        frag->fh_fsize = headerSize;
        frag->fh_owner = IPDEST_FRAGERR;
        TAILQ_INIT(&frag->fh_frags);
        if (ifb->fraglist_timer_on == 0) {
            ifb->fraglist_timer_on = 1;
            timeout(sip_fragtimer, ifb, hz/2);
        }
        TAILQ_INSERT_TAIL(&ifb->fraglist, frag, fh_link);
    }
    m->m_nextpkt = NULL;
    *(frag->fh_frags.tqh_last) = m;
    frag->fh_frags.tqh_last = &m->m_nextpkt;
}

/* Handle our filter being detached */
int ipv4_detach(caddr_t  cookie)
{
    struct blueCtlBlock *ifb = (struct blueCtlBlock*)cookie;
    
    ifb->ipv4_proto_filter_id = 0;
    
    if (!ifb->ipv4_stopping)
    {
        /*
         * We're being detached outside the context of
         * ipv4_stop, call ipv4_stop to cleanup.
         */
        ipv4_stop(ifb);
    }
    
    return 0;
}

/* 
 * Hand an IP packet from classic to the firewall to let the
 * firewall process it.
 * Returns 0 if the firewall lets the packet by, non-zero and sets
 * *m_orig to NULL if the packet was rejected or rerouted.
 */
int
process_firewall_out(struct mbuf **m_orig, struct ifnet	*ifp)
{
    struct ip			*ip;
    int					hlen;
    struct sockaddr_in 	sin, *dst = &sin;
#if DUMMYNET || IPDIVERT
    struct sockaddr_in  *old = dst;
#endif
    int					off;
    struct ip_fw_chain	*rule = NULL;
    
    if (ip_fw_chk_ptr == NULL)
        return 0;
    
    ip = mtod(*m_orig, struct ip*);
    hlen = ip->ip_hl << 2;
    dst->sin_family = AF_INET;
    dst->sin_len = sizeof(*dst);
    dst->sin_addr = ip->ip_dst;
    
    off = ip_fw_chk_ptr(&ip, hlen, ifp, &ip_divert_cookie,
                        m_orig, &rule, &dst);
    /*
     * On return we must do the following:
     * m == NULL         -> drop the pkt
     * 1<=off<= 0xffff   -> DIVERT
     * (off & 0x10000)   -> send to a DUMMYNET pipe
     * dst != old        -> IPFIREWALL_FORWARD
     * off==0, dst==old  -> accept
     * If some of the above modules is not compiled in, then
     * we should't have to check the corresponding condition
     * (because the ipfw control socket should not accept
     * unsupported rules), but better play safe and drop
     * packets in case of doubt.
     */
     /*
      * Default case, firewall accepted the packet.
      * We ignore off == 0 and dest == old check
      * because we don't support DUMMYNET or IPFIREWALL_FORWARD
      * in the kernel.
      */
#if DUMMYNET || IPDIVERT
    if (off == 0 && dst == old && *m_orig != NULL)
#else
    if (*m_orig != NULL)
#endif
        return FIREWALL_ACCEPTED;
    /* Firewall rejected (gobbled up) the packet. */
    if (*m_orig == NULL)
        return 1;
#if DUMMYNET
    if (off & 0x10000) {
        /*
         * pass the pkt to dummynet. Need to include
         * pipe number, m, ifp, ro, hlen because these are
         * not recomputed in the next pass.
         * All other parameters have been already used and
         * so they are not needed anymore.
         * XXX note: if the ifp or ro entry are deleted
         * while a pkt is in dummynet, we are in trouble!
         *
         * %%% TBD Need to compute ro
         */
        DBG_PRINTF(("sipfw_sosend dummynet\n"));
        dummynet_io(off & 0xffff, DN_TO_IP_OUT, *m_orig,ifp,ro,hlen,rule);
        *m_orig = NULL;
        return 2;
    }
#endif
#if IPDIVERT
    if (off > 0 && off < 0x10000) { /* Divert Packet */
        ip_divert_port = off & 0xffff;
        ip_porotox[IPPROTO_DIVERT]->pr_input(*m_orig, 0);
        *m_orig = NULL;
        return 3;
    }
#endif
    /* Nothing above matched, we must drop the packet. */
    m_freem(*m_orig);
    *m_orig = NULL;
    
    return 4;
}

/* 
 * Hand an IP packet from the interface to classic to the firewall
 * to let the firewall process it.
 * Returns 0 if the firewall lets the packet by, non-zero and sets
 * *m_orig to NULL if the packet was rejected or rerouted.
 */
int
process_firewall_in(struct mbuf **m_orig)
{
    struct ip			*ip;
    int					hlen;
    int					off;
    struct ip_fw_chain	*rule = NULL ;
    
    /*
     * If there is no firewall or we've been "forwarded from the
     * output side", skip the firewall the second time.
     */
    if (ip_fw_chk_ptr == NULL || ip_fw_fwd_addr != NULL)
        return FIREWALL_ACCEPTED;
    
    ip = mtod(*m_orig, struct ip*);
    hlen = ip->ip_hl << 2;
    
    off = ip_fw_chk_ptr(&ip, hlen, NULL, &ip_divert_cookie,
                        m_orig, &rule, &ip_fw_fwd_addr);
    /*
     * On return we must do the following:
     * m == NULL         -> drop the pkt
     * 1<=off<= 0xffff   -> DIVERT
     * (off & 0x10000)   -> send to a DUMMYNET pipe
     * ip_fw_fwd_addr!=NULL -> IPFIREWALL_FORWARD
     * off==0, dst==old  -> accept
     * If some of the above modules is not compiled in, then
     * we should't have to check the corresponding condition
     * (because the ipfw control socket should not accept
     * unsupported rules), but better play safe and drop
     * packets in case of doubt.
     */
     /* Common case, firewall accepted the packet.
      * Since we don't support DUMMYNET or IPFIREWALL_FORWARD
      * We'll just ignore "off"
      */
#if DUMMYNET || IPFIREWALL_FORWARD || IPDIVERT
    if (off == 0 && ip_fw_fwd_addr == NULL && *m_orig != NULL)
#else
    if (*m_orig != NULL)
#endif
        return 0;
    /* Firewall rejected (gobbled up) the packet. */
    if (*m_orig == NULL)
        return 1;
#if IPFIREWALL_FORWARD
    /* Hack: We don't foward packets on their way to Classic */
    /* This probably leaks packets */
    if (ip_fw_fwd_addr) {
#warning There is probably a leak here!
        ip_fw_fwd_addr = NULL;
        return 0;
    }
#endif
#if DUMMYNET
    if (off & 0x10000) {
        /*
         * normally we would pass the pkt to dummynet.
         * Since this might be a duplicate (ICMP) and we already
         * know it's supposed to go to classic, we don't support
         * this here.
         */
/*      dummynet_io(off & 0xffff, DN_TO_IP_OUT, *m_orig,ifp,ro,hlen,rule);*/
#if SIP_DEBUG_ERR
        log(LOG_WARNING, "SharedIP: process_firewall_in, firewall wants to dummynet the packet!\n");
#endif
        m_freem(*m_orig);
        *m_orig = NULL;
        return 2;
    }
#endif
#if IPDIVERT
    if (off > 0 && off < 0x10000) { /* Divert Packet */
/*      ip_divert_port = off & 0xffff; */
/*      ip_porotox[IPPROTO_DIVERT]->pr_input(*m_orig, 0); */
#if SIP_DEBUG_ERR
        log(LOG_WARNING, "SharedIP: process_firewall_in, firewall wants to divert the packet!\n");
#endif
        *m_orig = NULL;
        return 3;
    }
#endif
    /* Nothing above matched, we must drop the packet. */
    m_freem(*m_orig);
    *m_orig = NULL;
    
    return 4;
}
