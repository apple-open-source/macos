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
#include "ppp_ipv6.h"
#include "ppp_if.h"
#include "if_ppplink.h"
#include "ppp_domain.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

static int ppp_ipv6_input(struct mbuf *m, char *frame_header, struct ifnet *ifp,
                        u_long dl_tag, int sync_ok);
static int ppp_ipv6_preoutput(struct ifnet *ifp, struct mbuf **m0, struct sockaddr *dst_netaddr,
                            caddr_t route, char *type, char *edst, u_long dl_tag );
static int ppp_ipv6_ioctl(u_long dl_tag, struct ifnet *ifp, u_long cmd, caddr_t data);


/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
init function
----------------------------------------------------------------------------- */
int ppp_ipv6_init(int init_arg)
{
    struct dlil_protomod_reg_str  reg;

    bzero(&reg, sizeof(struct dlil_protomod_reg_str));
    reg.attach_proto = ppp_ipv6_attach;
    reg.detach_proto = ppp_ipv6_detach;
    return dlil_reg_proto_module(PF_INET6, APPLE_IF_FAM_PPP, &reg);
}

/* -----------------------------------------------------------------------------
terminate function
----------------------------------------------------------------------------- */
int ppp_ipv6_dispose(int term_arg)
{
    return dlil_dereg_proto_module(PF_INET6, APPLE_IF_FAM_PPP);
}

/* -----------------------------------------------------------------------------
attach the PPPx interface ifp to the network protocol IPv6,
called when the ppp interface is ready for ppp traffic
----------------------------------------------------------------------------- */
int ppp_ipv6_attach(struct ifnet *ifp, u_long *dl_tag)
{
    int 			ret;
    struct dlil_proto_reg_str   reg;
    struct dlil_demux_desc      desc;
    u_short	      		ipv6proto = PPP_IPV6;
    struct ppp_fam		*fam = (struct ppp_fam *)ifp->family_cookie;
    
    LOGDBG(ifp, (LOGVAL, "ppp_ip_attach: name = %s, unit = %d\n", ifp->if_name, ifp->if_unit));

    if (fam->ipv6_tag) 
        return 0;	// already attached

    bzero(&reg, sizeof(struct dlil_proto_reg_str));
    // register demux structure for ppp protocols
    TAILQ_INIT(&reg.demux_desc_head);
    bzero(&desc, sizeof(struct dlil_demux_desc));
    desc.type = DLIL_DESC_RAW;
    desc.native_type = (char *) &ipv6proto;
    TAILQ_INSERT_TAIL(&reg.demux_desc_head, &desc, next);

    reg.protocol_family  = PF_INET6;
    reg.interface_family = ifp->if_family;
    reg.unit_number      = ifp->if_unit;
    reg.input            = ppp_ipv6_input;
    reg.ioctl            = ppp_ipv6_ioctl;
    reg.pre_output       = ppp_ipv6_preoutput;
    ret = dlil_attach_protocol(&reg, dl_tag);
    LOGRETURN(ret, ret, "ppp_ipv6_attach: dlil_attach_protocol error = 0x%x\n");
     
    LOGDBG(ifp, (LOGVAL, "ppp_ipv6_attach: dlil_attach_protocol tag = 0x%x\n", *dl_tag));
    return 0;
}

/* -----------------------------------------------------------------------------
detach the PPPx interface ifp from the network protocol IPv6,
called when the ppp interface stops ip traffic
----------------------------------------------------------------------------- */
int ppp_ipv6_detach(struct ifnet *ifp, u_long dl_tag)
{
    int 		ret;
    struct ppp_fam	*fam = (struct ppp_fam *)ifp->family_cookie;

    LOGDBG(ifp, (LOGVAL, "ppp_ipv6_detach\n"));

    if (!fam->ipv6_tag)
        return 0;	// already detached

    ret = dlil_detach_protocol(fam->ipv6_tag);
    LOGRETURN(ret, ret, "ppp_ipv6_detach: dlil_detach_protocol error = 0x%x\n");

    fam->ipv6_tag = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
called from dlil when an ioctl is sent to the interface
----------------------------------------------------------------------------- */
int ppp_ipv6_ioctl(u_long dl_tag, struct ifnet *ifp, u_long cmd, caddr_t data)
{
    struct ifaddr 	*ifa = (struct ifaddr *)data;
    int 		error = 0;

    switch (cmd) {

        case SIOCSIFADDR:
        case SIOCAIFADDR:
            LOGDBG(ifp, (LOGVAL, "ppp_ipv6_ioctl: cmd = SIOCSIFADDR/SIOCAIFADDR\n"));

            // only an IPv6 address should arrive here
            if (ifa->ifa_addr->sa_family != AF_INET6) {
                error = EAFNOSUPPORT;
                break;
            }
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
int ppp_ipv6_input(struct mbuf *m, char *frame_header, struct ifnet *ifp,
                 u_long dl_tag, int sync_ok)
{
    int  s;
    
    LOGMBUF("ppp_ipv6_input", m);

    schednetisr(NETISR_IPV6);

    /* Put the packet on the ip input queue */
    s = splimp();
    if (IF_QFULL(&ip6intrq)) {
        IF_DROP(&ip6intrq);
        splx(s);
        LOGDBG(ifp, (LOGVAL, "ppp%d: IPv6 input queue full\n", ifp->if_unit));
        ifp->if_iqdrops++;
        return 1;
    }
    IF_ENQUEUE(&ip6intrq, m);
    splx(s);

    return 0;
}

/* -----------------------------------------------------------------------------
pre_output function
----------------------------------------------------------------------------- */
int ppp_ipv6_preoutput(struct ifnet *ifp, struct mbuf **m0, struct sockaddr *dst_netaddr,
                     caddr_t route, char *type, char *edst, u_long dl_tag)
{

    LOGMBUF("ppp_ipv6_preoutput", *m0);

    *(u_int16_t *)type = PPP_IPV6;
    return 0;
}

