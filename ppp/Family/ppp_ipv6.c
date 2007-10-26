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

#include "ppp_defs.h"		// public ppp values
#include "ppp_ipv6.h"
#include "ppp_domain.h"
#include "ppp_if.h"
#include "if_ppplink.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

static errno_t ppp_ipv6_input(ifnet_t ifp, protocol_family_t protocol,
									 mbuf_t packet, char* header);
static errno_t ppp_ipv6_preoutput(ifnet_t ifp, protocol_family_t protocol,
									mbuf_t *packet, const struct sockaddr *dest, 
									void *route, char *frame_type, char *link_layer_dest);
static errno_t ppp_ipv6_ioctl(ifnet_t ifp, protocol_family_t protocol,
									 u_int32_t command, void* argument);


/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */
extern lck_mtx_t	*ppp_domain_mutex;

/* -----------------------------------------------------------------------------
init function
----------------------------------------------------------------------------- */
int ppp_ipv6_init(int init_arg)
{
	return proto_register_plumber(PF_INET6, APPLE_IF_FAM_PPP, ppp_ipv6_attach, ppp_ipv6_detach);
}

/* -----------------------------------------------------------------------------
terminate function
----------------------------------------------------------------------------- */
int ppp_ipv6_dispose(int term_arg)
{
	proto_unregister_plumber(PF_INET6, APPLE_IF_FAM_PPP);
	return 0;
}

/* -----------------------------------------------------------------------------
attach the PPPx interface ifp to the network protocol IPv6,
called when the ppp interface is ready for ppp traffic
----------------------------------------------------------------------------- */
errno_t ppp_ipv6_attach(ifnet_t ifp, protocol_family_t protocol)
{
    int					ret;
    struct ifnet_attach_proto_param   reg;
    struct ppp_if		*wan = (struct ppp_if *)ifnet_softc(ifp);
    
    LOGDBG(ifp, (LOGVAL, "ppp_ipv6_attach: name = %s, unit = %d\n", ifnet_name(ifp), ifnet_unit(ifp)));

    if (wan->ipv6_attached) 
        return 0;	// already attached

    bzero(&reg, sizeof(struct ifnet_attach_proto_param));
	
	reg.input = ppp_ipv6_input;
	reg.pre_output = ppp_ipv6_preoutput;
	reg.ioctl = ppp_ipv6_ioctl;
	ret = ifnet_attach_protocol(ifp, PF_INET6, &reg);
    LOGRETURN(ret, ret, "ppp_ipv6_attach: ifnet_attach_protocol error = 0x%x\n");
     
    LOGDBG(ifp, (LOGVAL, "ppp_ipv6_attach: ifnet_attach_protocol family = 0x%x\n", protocol));
    wan->ipv6_attached = 1;

    return 0;
}

/* -----------------------------------------------------------------------------
detach the PPPx interface ifp from the network protocol IPv6,
called when the ppp interface stops ip traffic
----------------------------------------------------------------------------- */
void ppp_ipv6_detach(ifnet_t ifp, protocol_family_t protocol)
{
    int 		ret;
    struct ppp_if		*wan = (struct ppp_if *)ifnet_softc(ifp);

    LOGDBG(ifp, (LOGVAL, "ppp_ipv6_detach\n"));

    if (!wan->ipv6_attached)
        return;	// already detached

    ret = ifnet_detach_protocol(ifp, PF_INET6);
	if (ret)
        log(LOGVAL, "ppp_ipv6_detach: ifnet_detach_protocol error = 0x%x\n", ret);

    wan->ipv6_attached = 0;
}

/* -----------------------------------------------------------------------------
called from dlil when an ioctl is sent to the interface
----------------------------------------------------------------------------- */
errno_t ppp_ipv6_ioctl(ifnet_t ifp, protocol_family_t protocol,
									 u_int32_t command, void* argument)
{
    struct ifaddr 	*ifa = (struct ifaddr *)argument;
    int 		error = 0;

    switch (command) {

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
errno_t ppp_ipv6_input(ifnet_t ifp, protocol_family_t protocol,
									 mbuf_t packet, char* header)
{
    LOGMBUF("ppp_ipv6_input", packet);

	if (proto_input(PF_INET6, packet))
		mbuf_freem(packet);

	return 0;
}

/* -----------------------------------------------------------------------------
pre_output function
----------------------------------------------------------------------------- */
errno_t ppp_ipv6_preoutput(ifnet_t ifp, protocol_family_t protocol,
									mbuf_t *packet, const struct sockaddr *dest, 
									void *route, char *frame_type, char *link_layer_dest)
{

    LOGMBUF("ppp_ipv6_preoutput", *packet);

    *(u_int16_t *)frame_type = PPP_IPV6;
    return 0;
}

