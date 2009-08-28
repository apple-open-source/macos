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

#include <net/dlil.h>
#include <net/if.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/kpi_protocol.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_arp.h>
#include <sys/socketvar.h>

#include "firewire.h"
#include "if_firewire.h"
}
#include "IOFireWireIP.h"

extern void firewire_arpintr __P((mbuf_t	m));

extern errno_t firewire_inet_arp __P((ifnet_t ifp, 
						u_short arpop, 
						const struct sockaddr_dl	*sender_hw,
						const struct sockaddr		*sender_proto, 
						const struct sockaddr_dl	*target_hw, 
						const struct sockaddr		*target_proto));
						
extern void firewire_inet_event __P((ifnet_t			ifp, 
							__unused protocol_family_t	protocol, 
							const struct kev_msg		*event));

////////////////////////////////////////////////////////////////////////////////
//
// inet_firewire_input
//
// IN: struct mbuf  *m, char *frame_header, ifnet_t ifp, 
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
static errno_t
inet_firewire_input(
	__unused ifnet_t			ifp,
	__unused protocol_family_t	protocol_family,
	mbuf_t						m,
	char     					*frame_header)
{
    struct firewire_header *eh = (struct firewire_header *)frame_header;
    u_short fw_type;
	
	ifnet_touch_lastchange(ifp);

    fw_type = ntohs(eh->fw_type);

    switch (fw_type) 
	{
        case FWTYPE_IP:
			{
				mbuf_pullup(&m, sizeof(struct ip));
				if (m == NULL)
					return EJUSTRETURN;
				
				errno_t ret = proto_input(PF_INET, m);
				
				if( ret )
					mbuf_freem(m);

				return ret;
			}
			
        case FWTYPE_ARP:
            firewire_arpintr(m);
			break;

        default:
            return ENOENT;
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
//  inet_firewire_pre_output
//
//   IN:	ifnet_t ifp
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
inet_firewire_pre_output(
	ifnet_t						interface,
	__unused protocol_family_t	protocol_family,
	mbuf_t						*m0,
	const struct sockaddr		*dst_netaddr,
	void*						route,
	char						*type,
	char						*edst)
{
    mbuf_t m = *m0;
    errno_t	result = 0;
	    
    if ((ifnet_flags(interface) & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) 
		return ENETDOWN;
	
	// Tell firewire_frameout it's ok to loop packet unless negated below.
    mbuf_setflags(m, mbuf_flags(m) | M_LOOP);

    switch (dst_netaddr->sa_family) 
	{
    	case AF_INET: 
		{
			struct sockaddr_dl	ll_dest;
			result = inet_arp_lookup(interface, (const struct sockaddr_in*)dst_netaddr,
								   &ll_dest, sizeof(ll_dest), (route_t)route, *m0);
			if (result == 0) 
			{
				bcopy(LLADDR(&ll_dest), edst, FIREWIRE_ADDR_LEN);
				*(u_int16_t*)type = htons(FWTYPE_IP);
			}
		}
		break;

        case AF_UNSPEC:
		{
            mbuf_setflags(m, mbuf_flags(m) & ~M_LOOP);
            register struct firewire_header *fwh = (struct firewire_header *)dst_netaddr->sa_data;
			(void)memcpy(edst, fwh->fw_dhost, FIREWIRE_ADDR_LEN);
            *(u_short *)type = fwh->fw_type;
		}
		break;

        default:
            return EAFNOSUPPORT;
    }

	return result;
}

////////////////////////////////////////////////////////////////////////////////
//
//  firewire_inet_prmod_ioctl
//
//   IN:	ifnet_t ifp
//   IN:	unsigned long	command
//   IN:	caddr_t			data
// 
// Invoked by : 
//  Invoked by dlil.c for dlil_output=>(*proto)->dl_pre_output=> 
//  inet_firewire_pre_output=>
//
// Process an ioctl from dlil_ioctl in the context of ip, i guess !! 
//
////////////////////////////////////////////////////////////////////////////////
static errno_t
firewire_inet_prmod_ioctl(
    __unused ifnet_t			ifp,
    __unused protocol_family_t	protocol_family,
    __unused unsigned long		command,
    __unused void*				data)
{
    return EOPNOTSUPP;
}

static errno_t
firewire_inet_resolve_multi(
	ifnet_t					ifp,
	const struct sockaddr	*proto_addr,
	struct sockaddr_dl		*out_ll,
	size_t					ll_len)
{
	static const size_t minsize = offsetof(struct sockaddr_dl, sdl_data[0]) + FIREWIRE_ADDR_LEN;
	const struct sockaddr_in	*sin = (const struct sockaddr_in*)proto_addr;

	if (proto_addr->sa_family != AF_INET)
		return EAFNOSUPPORT;
	
	if (proto_addr->sa_len < sizeof(struct sockaddr_in))
		return EINVAL;

	if (ll_len < minsize)
		return EMSGSIZE;
	
	bzero(out_ll, minsize);
	out_ll->sdl_len = minsize;
	out_ll->sdl_family = AF_LINK;
	out_ll->sdl_index = ifnet_index(ifp);
	out_ll->sdl_type = IFT_IEEE1394;
	out_ll->sdl_nlen = 0;
	out_ll->sdl_alen = FIREWIRE_ADDR_LEN;
	out_ll->sdl_slen = 0;
	FIREWIRE_MAP_IP_MULTICAST(&sin->sin_addr, LLADDR(out_ll));
	
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// firewire_attach_inet
//
//   IN:	ifnet_t ifp
//   
// Invoked by:
//  firewire_attach_inet will be invoked from IOFWInterface::attachToDataLinkLayer
//
////////////////////////////////////////////////////////////////////////////////
int
firewire_attach_inet(ifnet_t ifp, protocol_family_t protocol_family)
{
	struct ifnet_attach_proto_param	proto;
	struct ifnet_demux_desc demux[2];
    u_short en_native=htons(FWTYPE_IP);
    u_short arp_native=htons(FWTYPE_ARP);
	errno_t	error;
	
	bzero(&demux[0], sizeof(demux));
	demux[0].type	= DLIL_DESC_ETYPE2;
	demux[0].data	= &en_native;
	demux[0].datalen = sizeof(en_native);
	demux[1].type	= DLIL_DESC_ETYPE2;
	demux[1].data	= &arp_native;
	demux[1].datalen = sizeof(arp_native);
	
	bzero(&proto, sizeof(proto));
	proto.demux_list	= demux;
	proto.demux_count	= sizeof(demux) / sizeof(demux[0]);
	proto.input			= inet_firewire_input;
	proto.pre_output	= inet_firewire_pre_output;
	proto.ioctl			= firewire_inet_prmod_ioctl;
	proto.event			= firewire_inet_event;
	proto.resolve		= firewire_inet_resolve_multi;
	proto.send_arp		= firewire_inet_arp;
	
	error = ifnet_attach_protocol(ifp, protocol_family, &proto);
	if (error && error != EEXIST) 
	{
		printf("WARNING: firewire_attach_inet can't attach ip to %s%d\n",
			   ifnet_name(ifp), ifnet_unit(ifp));
	}
	
	return error;
}