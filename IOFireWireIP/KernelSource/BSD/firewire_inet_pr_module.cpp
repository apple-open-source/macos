/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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


#ifndef INET
#define INET 1
#endif

extern "C"{
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <sys/socketvar.h>

#include <net/dlil.h>

#include "firewire.h"
#include "if_firewire.h"
extern void ipintr(void);

}

#include "IOFireWireIP.h"

#if BRIDGE
#include <net/bridge.h>
#endif

/* #include "vlan.h" */
#if NVLAN > 0
#include <net/if_vlan_var.h>
#endif /* NVLAN > 0 */

#define IFP2AC(IFP) ((struct arpcom *)IFP)

extern void firewire_arpintr __P((struct mbuf *));
extern int	firewire_arpresolve __P((struct arpcom *, struct rtentry *, struct mbuf *,
									struct sockaddr *, u_char *, struct rtentry *));

////////////////////////////////////////////////////////////////////////////////
//
// inet_firewire_input
//
// IN: struct mbuf  *m, char *frame_header, struct ifnet *ifp, 
// IN: u_long dl_tag, int sync_ok
// 
// Invoked by : 
//  It will be called from the context of dlil_input_thread queue from
//  dlil_input_packet
// 
// Process a received firewire ARP/IP packet, the packet is in the mbuf 
// chain m
//
////////////////////////////////////////////////////////////////////////////////
int
inet_firewire_input(struct mbuf  *m, char *frame_header, struct ifnet *ifp, u_long dl_tag, 						int sync_ok)
{
    register struct firewire_header *eh = (struct firewire_header *) frame_header;
    register struct ifqueue *inq=0;
    u_short ether_type;
    int s;
    u_int16_t ptype = 0;
	
    if ((ifp->if_flags & IFF_UP) == 0) {
        m_freem(m);
        return EJUSTRETURN;
    }
//  log(LOG_DEBUG,"inet_firewire_input %d\n", __LINE__);

    getmicrotime(&ifp->if_lastchange);

    if (m->m_flags & (M_BCAST|M_MCAST))
        ifp->if_imcasts++;

    ether_type = ntohs(eh->ether_type);

    switch (ether_type) {

        case ETHERTYPE_IP:
            if (ipflow_fastforward(m))
                return EJUSTRETURN;

 //        log(LOG_DEBUG,"inet_firewire_input %d\n", __LINE__);
            
           	ptype = mtod(m, struct ip *)->ip_p;
            if ((sync_ok == 0) || (ptype != IPPROTO_TCP && ptype != IPPROTO_UDP)) 
            {
                schednetisr(NETISR_IP); 
            }

	        // schednetisr(NETISR_IP);
            // Assign the ipintrq for the incoming queue
            inq = &ipintrq;
            break;

        case ETHERTYPE_ARP:
			// Direct call to process the arp packet, no NETISR required
            firewire_arpintr(m);
            inq = 0;
            return 0;  

        default: {
            return ENOENT;
        }
    }

    if (inq == 0)
        return ENOENT;

	// Protect the queue
    s = splimp();
    
	if (IF_QFULL(inq)) 
    {
		IF_DROP(inq);
		m_freem(m);
		splx(s);
		return EJUSTRETURN;
	} else
		IF_ENQUEUE(inq, m);
        
	splx(s);

    if ((sync_ok) && (ptype == IPPROTO_TCP || ptype == IPPROTO_UDP)) 
    {
        s = splnet();
        ipintr();
        splx(s);
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
//  inet_firewire_pre_output
//
//   IN:	struct ifnet *ifp
//   IN:	struct mbuf **m0
//   IN:	struct sockaddr dst_netaddr
//   IN:	caddr_t	route
//   OUT:	char *type
//	 OUT:	char *edst
//	 IN:	u_long dl_tag
// 
// Invoked by : 
//  Invoked by dlil.c for dlil_output=>(*proto)->dl_pre_output=> 
//  inet_firewire_pre_output=>
//
// Process a received firewire ARP/IP packet, the packet is in the mbuf 
// chain m
//
////////////////////////////////////////////////////////////////////////////////
int
inet_firewire_pre_output(struct ifnet *ifp, struct mbuf **m0, struct sockaddr *dst_netaddr,
            caddr_t route, char *type, char *edst, u_long dl_tag)
{
    struct rtentry  *rt0 = (struct rtentry *) route;
    // int s;
    register struct mbuf *m = *m0;
    register struct rtentry *rt;
    register struct firewire_header *eh;
    int off ; // , len = m->m_pkthdr.len;
    struct arpcom *ac = IFP2AC(ifp);

	//log(LOG_DEBUG,"inet_firewire_pre_output called\n");

    if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) 
	{
		// log(LOG_DEBUG,"Interface not IFF_UP|IFF_RUNNING\n");
        return ENETDOWN;
	}

    rt = rt0;
    
    // begin validating rt
    if (rt) {
        if ((rt->rt_flags & RTF_UP) == 0) {
            rt0 = rt = rtalloc1(dst_netaddr, 1, 0UL);
            if (rt0)
                rtunref(rt);
            else{
				// log(LOG_DEBUG,"inet_firewire_pre_output EHOSTUNREACH\n");
				return EHOSTUNREACH;
			}
        }

        if (rt->rt_flags & RTF_GATEWAY) {
            if (rt->rt_gwroute == 0)
                goto lookup;
            if (((rt = rt->rt_gwroute)->rt_flags & RTF_UP) == 0) {
                rtfree(rt); rt = rt0;
                lookup: rt->rt_gwroute = rtalloc1(rt->rt_gateway, 1, 0UL);
				if ((rt = rt->rt_gwroute) == 0) {
					// log(LOG_DEBUG,"inet_firewire_pre_output rt->rt_gwroute : EHOSTUNREACH\n");
					return (EHOSTUNREACH);
				}
            }
        }

	
        if (rt->rt_flags & RTF_REJECT)
            if (rt->rt_rmx.rmx_expire == 0 || (u_long)time_second < rt->rt_rmx.rmx_expire) {
				// log(LOG_DEBUG,"inet_firewire_pre_output : EHOSTDOWN : EHOSTUNREACH\n");
                return (rt == rt0 ? EHOSTDOWN : EHOSTUNREACH);
			}
    }
    // end validating rt
    
	// log(LOG_DEBUG,"inet_firewire_pre_output : end validating rt\n");
	
    //
    // Tell firewire_frameout it's ok to loop packet unless negated below.
    //
    m->m_flags |= M_LOOP;

    switch (dst_netaddr->sa_family) {
        
        case AF_INET:
            if (!firewire_arpresolve(ac, rt, m, dst_netaddr, (u_char*)edst, rt0))
                return (EJUSTRETURN);	// if not yet resolved 
            off = m->m_pkthdr.len - m->m_len;
            *(u_short *)type = htons(ETHERTYPE_IP);
            break;

        case AF_UNSPEC:
            m->m_flags &= ~M_LOOP;
            eh = (struct firewire_header *)dst_netaddr->sa_data;
			(void)memcpy(edst, eh->ether_dhost, FIREWIRE_ADDR_LEN);
            *(u_short *)type = eh->ether_type;
            break;

        default:
            log(LOG_DEBUG,"%s%d: can't handle af%d\n", ifp->if_name, 
                                    ifp->if_unit, dst_netaddr->sa_family);

            return EAFNOSUPPORT;
    }

	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
//  firewire_inet_prmod_ioctl
//
//   IN:	u_long       dl_tag
//   IN:	struct ifnet *ifp
//   IN:	int          command
//   IN:	caddr_t      data
// 
// Invoked by : 
//  Invoked by dlil.c for dlil_output=>(*proto)->dl_pre_output=> 
//  inet_firewire_pre_output=>
//
// Process an ioctl from dlil_ioctl in the context of ip, i guess !! 
//
////////////////////////////////////////////////////////////////////////////////
int
firewire_inet_prmod_ioctl(u_long dl_tag, struct ifnet *ifp, long unsigned int command, caddr_t data)
{
    int error = EOPNOTSUPP;

    return (error);
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_attach_inet
//
//   IN		:	struct ifnet *ifp
//   IN/OUT	:	u_long		 *dl_tag
//
// Invoked by:
//  firewire_attach_inet will be invoked from IOFWInterface::attachToDataLinkLayer
//
////////////////////////////////////////////////////////////////////////////////
int
firewire_attach_inet(struct ifnet *ifp, u_long *dl_tag)
{
    struct dlil_proto_reg_str   reg;
    struct dlil_demux_desc      desc;
    struct dlil_demux_desc      desc2;
	
    u_long	ip_dl_tag  = 0;
	u_short en_native  = ETHERTYPE_IP;
    u_short arp_native = ETHERTYPE_ARP;
    
	int	stat;

	//log(LOG_DEBUG, "firewire_attach_inet+\n");

    stat = dlil_find_dltag(ifp->if_family, ifp->if_unit, PF_INET, &ip_dl_tag);
    
    if (stat == 0)
	{
		//log(LOG_DEBUG,"IGNORE: firewire_attach_inet found a stat %d, ip_dl_tag %d \n", stat, ip_dl_tag);
		*dl_tag = ip_dl_tag;
		//log(LOG_DEBUG, "firewire_attach_inet- %d\n", __LINE__);
        return stat;
	}

    bzero(&reg, sizeof(struct dlil_proto_reg_str));

    TAILQ_INIT(&reg.demux_desc_head);
    desc.type = DLIL_DESC_RAW;
    desc.variants.bitmask.proto_id_length = 0;
    desc.variants.bitmask.proto_id = 0;
    desc.variants.bitmask.proto_id_mask = 0;
    desc.native_type = (u_char*)&en_native;
    TAILQ_INSERT_TAIL(&reg.demux_desc_head, &desc, next);
    reg.interface_family = ifp->if_family;
    reg.unit_number      = ifp->if_unit;
    reg.input            = inet_firewire_input;
    reg.pre_output       = inet_firewire_pre_output;
    reg.event            = 0;
    reg.offer            = 0;
    reg.ioctl            = firewire_inet_prmod_ioctl;
    reg.default_proto    = 1;
    reg.protocol_family  = PF_INET;

    desc2 = desc;
    desc2.native_type = (u_char *) &arp_native;
    TAILQ_INSERT_TAIL(&reg.demux_desc_head, &desc2, next);
	
    stat = dlil_attach_protocol(&reg, &ip_dl_tag);
    if(stat) 
	{
		log(LOG_DEBUG,"WARNING: firewire_attach_inet can't attach ip to interface %d  PF %x IF %x\n", 
					stat, reg.protocol_family, reg.interface_family);
        return stat;
    }
	*dl_tag = ip_dl_tag;

	//log(LOG_DEBUG, "firewire_attach_inet- %d\n", __LINE__);
	
    return stat;
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_detach_inet
//
//   IN:	struct ifnet *ifp
//   
// Invoked by:
//  firewire_detach_inet will be invoked from IOFireWireIP::nicDetach
//
////////////////////////////////////////////////////////////////////////////////
int  firewire_detach_inet(struct ifnet *ifp, u_long dl_tag)
{
//    u_long      ip_dl_tag = 0;
    int         stat;

#ifdef FIREWIRETODO
    stat = dlil_find_dltag(ifp->if_family, ifp->if_unit, PF_INET, &ip_dl_tag);
    if (stat == 0) 
	{
#endif

	stat = dlil_detach_protocol(dl_tag);
	if (stat) 
	{
		kprintf("WARNING: firewire_detach_inet can't detach ip from interface\n");
	}

#ifdef FIREWIRETODO
    }
#endif
    
    return stat;
}

