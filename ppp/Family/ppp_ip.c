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

#include <net/if.h>
#include <net/dlil.h>
#include <net/netisr.h>
#include <machine/spl.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>

#include "ppp_defs.h"		// public ppp values
#include "ppp_fam.h"
#include "ppp_ip.h"
#include "ppp_if.h"
#include "if_ppplink.h"
#include "ppp_domain.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

static int ppp_ip_input(struct mbuf *m, char *frame_header, struct ifnet *ifp,
                        u_long dl_tag, int sync_ok);
static int ppp_ip_preoutput(struct ifnet *ifp, struct mbuf **m0, struct sockaddr *dst_netaddr,
                            caddr_t route, char *type, char *edst, u_long dl_tag );
static int ppp_ip_ioctl(u_long dl_tag, struct ifnet *ifp, u_long cmd, caddr_t data);


/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
init function
----------------------------------------------------------------------------- */
int ppp_ip_init(int init_arg)
{
    struct dlil_protomod_reg_str  reg;

    bzero(&reg, sizeof(struct dlil_protomod_reg_str));
    reg.attach_proto = ppp_ip_attach;
    reg.detach_proto = ppp_ip_detach;
    return dlil_reg_proto_module(PF_INET, APPLE_IF_FAM_PPP, &reg);
}

/* -----------------------------------------------------------------------------
terminate function
----------------------------------------------------------------------------- */
int ppp_ip_dispose(int term_arg)
{
    return dlil_dereg_proto_module(PF_INET, APPLE_IF_FAM_PPP);
}

/* -----------------------------------------------------------------------------
attach the PPPx interface ifp to the network protocol IP,
called when the ppp interface is ready for ppp traffic
----------------------------------------------------------------------------- */
int ppp_ip_attach(struct ifnet *ifp, u_long *dl_tag)
{
    int 			ret;
    struct dlil_proto_reg_str   reg;
    struct dlil_demux_desc      desc;
    u_short	      		ipproto = PPP_IP;
    struct ppp_fam		*fam = (struct ppp_fam *)ifp->family_cookie;
    
    LOGDBG(ifp, (LOGVAL, "ppp_ip_attach: name = %s, unit = %d\n", ifp->if_name, ifp->if_unit));

    if (fam->ip_tag) {
        *dl_tag = fam->ip_tag;
        return 0;	// already attached
    }

    bzero(&reg, sizeof(struct dlil_proto_reg_str));
    // register demux structure for ppp protocols
    TAILQ_INIT(&reg.demux_desc_head);
    bzero(&desc, sizeof(struct dlil_demux_desc));
    desc.type = DLIL_DESC_RAW;
    desc.native_type = (char *) &ipproto;
    TAILQ_INSERT_TAIL(&reg.demux_desc_head, &desc, next);

    reg.protocol_family  = PF_INET;
    reg.interface_family = ifp->if_family;
    reg.unit_number      = ifp->if_unit;
    reg.input            = ppp_ip_input;
    reg.ioctl            = ppp_ip_ioctl;
    reg.pre_output       = ppp_ip_preoutput;
    ret = dlil_attach_protocol(&reg, dl_tag);
    LOGRETURN(ret, ret, "ppp_attach_ip: dlil_attach_protocol error = 0x%x\n");
     
    LOGDBG(ifp, (LOGVAL, "ppp_attach_ip: dlil_attach_protocol tag = 0x%x\n", *dl_tag));
    return 0;
}

/* -----------------------------------------------------------------------------
detach the PPPx interface ifp from the network protocol IP,
called when the ppp interface stops ip traffic
----------------------------------------------------------------------------- */
int ppp_ip_detach(struct ifnet *ifp, u_long dl_tag)
{
    int 		ret;
    struct ppp_fam	*fam = (struct ppp_fam *)ifp->family_cookie;

    LOGDBG(ifp, (LOGVAL, "ppp_ip_detach\n"));

    if (!fam->ip_tag)
        return 0;	// already detached

    ret = dlil_detach_protocol(fam->ip_tag);
    LOGRETURN(ret, ret, "ppp_ip_detach: dlil_detach_protocol error = 0x%x\n");

    fam->ip_tag = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
called from dlil when an ioctl is sent to the interface
----------------------------------------------------------------------------- */
int ppp_ip_ioctl(u_long dl_tag, struct ifnet *ifp, u_long cmd, caddr_t data)
{
    struct ifaddr 	*ifa = (struct ifaddr *)data;
    //struct in_ifaddr 	*ia = (struct in_ifaddr *)data;
    struct ppp_fam	*fam = (struct ppp_fam *)ifp->family_cookie;
    struct sockaddr_in  *addr = (struct sockaddr_in *)ifa->ifa_addr;
    int 		error = 0;
    
    switch (cmd) {

        case SIOCSIFADDR:
        case SIOCAIFADDR:
            LOGDBG(ifp, (LOGVAL, "ppp_ip_ioctl: cmd = SIOCSIFADDR/SIOCAIFADDR\n"));

            // only an IPv4 address should arrive here
            if (ifa->ifa_addr->sa_family != AF_INET) {
                error = EAFNOSUPPORT;
                break;
            }
                            
            fam->ip_src.s_addr = addr->sin_addr.s_addr;
            /* 
                XXX very dirty...
                in.c doesn't pass the destination address to dlil
                but it happens to be the next address in the in_aliasreq
            */
            addr++;
            fam->ip_dst.s_addr = addr->sin_addr.s_addr;

            dlil_find_dltag(APPLE_IF_FAM_LOOPBACK, 0, PF_INET, &fam->ip_lotag);
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
int ppp_ip_input(struct mbuf *m, char *frame_header, struct ifnet *ifp,
                 u_long dl_tag, int sync_ok)
{
    int  s;
    
    LOGMBUF("ppp_ip_input", m);

    if (ipflow_fastforward(m)) {
        return 0;
    }

    schednetisr(NETISR_IP);

    /* Put the packet on the ip input queue */
    s = splimp();
    if (IF_QFULL(&ipintrq)) {
        IF_DROP(&ipintrq);
        splx(s);
        LOGDBG(ifp, (LOGVAL, "ppp%d: input queue full\n", ifp->if_unit));
        ifp->if_iqdrops++;
        return 1;
    }
    IF_ENQUEUE(&ipintrq, m);
    splx(s);

    return 0;
}

/* -----------------------------------------------------------------------------
pre_output function
----------------------------------------------------------------------------- */
int ppp_ip_preoutput(struct ifnet *ifp, struct mbuf **m0, struct sockaddr *dst_netaddr,
                     caddr_t route, char *type, char *edst, u_long dl_tag)
{
    int 		err;
    struct ppp_fam	*fam = (struct ppp_fam *)ifp->family_cookie;
    struct ppp_if      *wan = (struct ppp_if *)ifp->if_softc;

    LOGMBUF("ppp_ip_preoutput", *m0);

#if 0
    (*m0)->m_flags &= ~M_HIGHPRI;

    /* If this packet has the "low delay" bit set in the IP header,
     set priority bit for the packet. */
    ip = mtod(*m0, struct ip *);
    if (ip->ip_tos & IPTOS_LOWDELAY)
        (*m0)->m_flags |= M_HIGHPRI;
#endif

    if ((wan->sc_flags & SC_LOOP_LOCAL)
        && (((struct sockaddr_in *)dst_netaddr)->sin_addr.s_addr == fam->ip_src.s_addr)) {
        err = dlil_output(fam->ip_lotag, *m0, 0, dst_netaddr, 0);
        return (err ? err : EJUSTRETURN);
    }

    *(u_int16_t *)type = PPP_IP;
    return 0;
}

/* -----------------------------------------------------------------------------
Compare the source address of the packet with the source address of the interface
----------------------------------------------------------------------------- */
int ppp_ip_af_src_out(struct ifnet *ifp, char *pkt)
{
    struct ppp_fam	*fam = (struct ppp_fam *)ifp->family_cookie;
    struct ip 		*ip;
        
    ip = (struct ip *)pkt;
    return (ip->ip_src.s_addr != fam->ip_src.s_addr);
}

/* -----------------------------------------------------------------------------
Compare the source address of the packet with the dst address of the interface
----------------------------------------------------------------------------- */
int ppp_ip_af_src_in(struct ifnet *ifp, char *pkt)
{
    struct ppp_fam	*fam = (struct ppp_fam *)ifp->family_cookie;
    struct ip 		*ip;
        
    ip = (struct ip *)pkt;
    return (ip->ip_src.s_addr != fam->ip_dst.s_addr);
}


