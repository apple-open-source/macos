/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

/* -----------------------------------------------------------------------------
 *
 *  Theory of operation :
 *
 *  This file implements the ip protocol module for the ppp interface
 *
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
Includes
----------------------------------------------------------------------------- */

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sockio.h>
#include <kern/locks.h>

#include <net/if.h>
#include <net/kpi_protocol.h>
#include <machine/spl.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/bootp.h>

#include "ppp_defs.h"		// public ppp values
#include "ppp_ip.h"
#include "ppp_domain.h"
#include "ppp_if.h"
#include "if_ppplink.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

static errno_t ppp_ip_input(ifnet_t ifp, protocol_family_t protocol,
									 mbuf_t packet, char* header);
static errno_t ppp_ip_preoutput(ifnet_t ifp, protocol_family_t protocol,
									mbuf_t *packet, const struct sockaddr *dest, 
									void *route, char *frame_type, char *link_layer_dest);
static errno_t ppp_ip_ioctl(ifnet_t ifp, protocol_family_t protocol,
									 u_int32_t command, void* argument);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */
extern lck_mtx_t   *ppp_domain_mutex;

/* -----------------------------------------------------------------------------
init function
----------------------------------------------------------------------------- */
int ppp_ip_init(int init_arg)
{
    return proto_register_plumber(PF_INET, APPLE_IF_FAM_PPP,
							   ppp_ip_attach, ppp_ip_detach);
}

/* -----------------------------------------------------------------------------
terminate function
----------------------------------------------------------------------------- */
int ppp_ip_dispose(int term_arg)
{
    proto_unregister_plumber(PF_INET, APPLE_IF_FAM_PPP);
    return 0;
}

/* -----------------------------------------------------------------------------
attach the PPPx interface ifp to the network protocol IP,
called when the ppp interface is ready for ppp traffic
----------------------------------------------------------------------------- */
errno_t ppp_ip_attach(ifnet_t ifp, protocol_family_t protocol)
{
    int					ret;
    struct ifnet_attach_proto_param   reg;
    struct ppp_if		*wan = (struct ppp_if *)ifnet_softc(ifp);
    
    LOGDBG(ifp, (LOGVAL, "ppp_ip_attach: name = %s, unit = %d\n", ifnet_name(ifp), ifnet_unit(ifp)));

    if (wan->ip_attached) 
        return 0;	// already attached

    bzero(&reg, sizeof(struct ifnet_attach_proto_param));
	
	reg.input = ppp_ip_input;
	reg.pre_output = ppp_ip_preoutput;
	reg.ioctl = ppp_ip_ioctl;
	ret = ifnet_attach_protocol(ifp, PF_INET, &reg);
    LOGRETURN(ret, ret, "ppp_ip_attach: ifnet_attach_protocol error = 0x%x\n");
	
    LOGDBG(ifp, (LOGVAL, "ppp_i6_attach: ifnet_attach_protocol family = 0x%x\n", protocol));
	ifnet_find_by_name("lo0", &wan->lo_ifp);
	wan->ip_attached = 1;
	
    return 0;
}

/* -----------------------------------------------------------------------------
detach the PPPx interface ifp from the network protocol IP,
called when the ppp interface stops ip traffic
----------------------------------------------------------------------------- */
void ppp_ip_detach(ifnet_t ifp, protocol_family_t protocol)
{
    int 		ret;
    struct ppp_if		*wan = (struct ppp_if *)ifnet_softc(ifp);

    LOGDBG(ifp, (LOGVAL, "ppp_ip_detach\n"));

    if (!wan->ip_attached)
        return;	// already detached

	ifnet_release(wan->lo_ifp);
	wan->lo_ifp = 0;

    ret = ifnet_detach_protocol(ifp, PF_INET);
	if (ret)
        log(LOGVAL, "ppp_ip_detach: ifnet_detach_protocol error = 0x%x\n", ret);

    wan->ip_attached = 0;
}

/* -----------------------------------------------------------------------------
called from dlil when an ioctl is sent to the interface
----------------------------------------------------------------------------- */
errno_t ppp_ip_ioctl(ifnet_t ifp, protocol_family_t protocol,
									 u_int32_t command, void* argument)
{
    struct ifaddr 	*ifa = (struct ifaddr *)argument;
    //struct in_ifaddr 	*ia = (struct in_ifaddr *)data;
    struct ppp_if		*wan = (struct ppp_if *)ifnet_softc(ifp);
    struct sockaddr_in  *addr = (struct sockaddr_in *)ifa->ifa_addr;
    int 		error = 0;
    
    switch (command) {

        case SIOCSIFADDR:
        case SIOCAIFADDR:
            LOGDBG(ifp, (LOGVAL, "ppp_ip_ioctl: cmd = SIOCSIFADDR/SIOCAIFADDR\n"));

            // only an IPv4 address should arrive here
            if (ifa->ifa_addr->sa_family != AF_INET) {
                error = EAFNOSUPPORT;
                break;
            }
                            
            wan->ip_src.s_addr = addr->sin_addr.s_addr;
            /* 
                XXX very dirty...
                in.c doesn't pass the destination address to dlil
                but it happens to be the next address in the in_aliasreq
            */
            addr++;
            wan->ip_dst.s_addr = addr->sin_addr.s_addr;
            break;

        default :
            error = EOPNOTSUPP;
    }
    
    return error;
}

/* -----------------------------------------------------------------------------
called from dlil when a packet from the interface is to be dispatched to
the specific network protocol attached by dl_tag.
the network protocol has been determined earlier by the demux function.
the packet is in the mbuf chain m without
the frame header, which is provided separately. (not used)
----------------------------------------------------------------------------- */
errno_t ppp_ip_input(ifnet_t ifp, protocol_family_t protocol,
									 mbuf_t packet, char* header)
{
    
    LOGMBUF("ppp_ip_input", packet);

    if (ipflow_fastforward((struct mbuf *)packet)) {
        return 0;
    }

	if (proto_input(PF_INET, packet))
		mbuf_freem(packet);
	
    return 0;
}

/* -----------------------------------------------------------------------------
pre_output function
----------------------------------------------------------------------------- */
errno_t ppp_ip_preoutput(ifnet_t ifp, protocol_family_t protocol,
									mbuf_t *packet, const struct sockaddr *dest, 
									void *route, char *frame_type, char *link_layer_dest)
{
    errno_t				err;
    struct ppp_if		*wan = (struct ppp_if *)ifnet_softc(ifp);

    LOGMBUF("ppp_ip_preoutput", *packet);
	
	lck_mtx_lock(ppp_domain_mutex);

#if 0
    (*packet)->m_flags &= ~M_HIGHPRI;

    /* If this packet has the "low delay" bit set in the IP header,
     set priority bit for the packet. */
    ip = mtod(*packet, struct ip *);
    if (ip->ip_tos & IPTOS_LOWDELAY)
        (*packet)->m_flags |= M_HIGHPRI;
#endif

    if ((wan->sc_flags & SC_LOOP_LOCAL)
        && (((struct sockaddr_in *)dest)->sin_addr.s_addr == wan->ip_src.s_addr)
		&& wan->lo_ifp) {
        err = ifnet_output(wan->lo_ifp, PF_INET, *packet, 0, (struct sockaddr *)dest);
		lck_mtx_unlock(ppp_domain_mutex);
        return (err ? err : EJUSTRETURN);
    }
	lck_mtx_unlock(ppp_domain_mutex);
    *(u_int16_t *)frame_type = PPP_IP;
    return 0;
}

/* -----------------------------------------------------------------------------
Compare the source address of the packet with the source address of the interface
----------------------------------------------------------------------------- */
int ppp_ip_af_src_out(ifnet_t ifp, char *pkt)
{
    struct ppp_if	*wan = (struct ppp_if *)ifnet_softc(ifp);
    struct ip 		*ip;
        
    ip = (struct ip *)pkt;
    return (ip->ip_src.s_addr != wan->ip_src.s_addr);
}

/* -----------------------------------------------------------------------------
Compare the source address of the packet with the dst address of the interface
----------------------------------------------------------------------------- */
int ppp_ip_af_src_in(ifnet_t ifp, char *pkt)
{
    struct ppp_if	*wan = (struct ppp_if *)ifnet_softc(ifp);
    struct ip 		*ip;
        
    ip = (struct ip *)pkt;
    return (ip->ip_src.s_addr != wan->ip_dst.s_addr);
}

/* -----------------------------------------------------------------------------
Check if the packet is a bootp packet for us
----------------------------------------------------------------------------- */
int ppp_ip_bootp_client_in(ifnet_t ifp, char *pkt)
{
    struct ppp_if	*wan = (struct ppp_if *)ifnet_softc(ifp);
    struct ip 		*ip;
	struct udphdr	*udp;
	
    ip = (struct ip *)pkt;
    if (ip->ip_dst.s_addr == wan->ip_src.s_addr && ip->ip_p == IPPROTO_UDP) {
	
		udp = (struct udphdr *)(pkt + sizeof(struct ip));
		if (udp->uh_sport == htons(IPPORT_BOOTPS) && udp->uh_dport == htons(IPPORT_BOOTPC)) {
				return 1;
		}
	}

	return 0;
}

/* -----------------------------------------------------------------------------
Check if the packet is a broadcast bootp packet
----------------------------------------------------------------------------- */
int ppp_ip_bootp_server_in(ifnet_t ifp, char *pkt)
{
    //struct ppp_if	*wan = (struct ppp_if *)ifnet_softc(ifp);
    struct ip 		*ip;
	struct udphdr	*udp;
	
    ip = (struct ip *)pkt;
    if (ip->ip_dst.s_addr == htonl(INADDR_BROADCAST) && ip->ip_p == IPPROTO_UDP) {
	
		udp = (struct udphdr *)(pkt + sizeof(struct ip));
		if (udp->uh_sport == htons(IPPORT_BOOTPC) && udp->uh_dport == htons(IPPORT_BOOTPS)) {
				return 1;
		}
	}

	return 0;
}


