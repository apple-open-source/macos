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
 *  This contains the family shim for PPP. This is really a shim, since all
 *  the work is done in the interface layer
 *
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Includes
----------------------------------------------------------------------------- */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/filio.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <net/dlil.h>
#include <mach/mach_types.h>
#include <machine/spl.h>
#include <sys/sockio.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/time.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_var.h>

#include "ppp_defs.h"		// public ppp values
#include "ppp_fam.h"
#include "ppp_ip.h"
#include "if_ppplink.h"		// public link API
#include "ppp_domain.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

static int ppp_fam_add_if(struct ifnet *ifp);
static int ppp_fam_del_if(struct ifnet *ifp);
static int ppp_fam_add_proto(struct ddesc_head_str *desc_head, struct if_proto *proto, u_long dl_tag);
static int ppp_fam_del_proto(struct if_proto *proto, u_long dl_tag);

static int ppp_fam_demux(struct ifnet *ifp, struct mbuf *m, char *frame_header,
                   void  *if_proto_ptr);
static int ppp_fam_frameout(struct ifnet *ifp, struct mbuf **m0,
                      struct sockaddr *ndest, char *edst, char *ppp_type);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
init function
----------------------------------------------------------------------------- */
int ppp_fam_init()
{
    int 			ret;
    struct dlil_ifmod_reg_str   mod;

    // register the module
    bzero(&mod, sizeof(struct dlil_ifmod_reg_str));
    mod.add_if = ppp_fam_add_if;
    mod.del_if = ppp_fam_del_if;
    mod.add_proto = ppp_fam_add_proto;
    mod.del_proto = ppp_fam_del_proto;
    ret = dlil_reg_if_modules(APPLE_IF_FAM_PPP, &mod);
    LOGRETURN(ret, ret, "ppp_fam_init: dlil_reg_if_modules error = 0x%x\n");

    return 0;
}

/* -----------------------------------------------------------------------------
terminate function
----------------------------------------------------------------------------- */
int ppp_fam_dispose()
{
    int ret;

    ret = dlil_dereg_if_modules(APPLE_IF_FAM_PPP);
    LOGRETURN(ret, ret, "ppp_fam_dispose: dlil_dereg_if_modules error = 0x%x\n");

    return 0;
}

/* -----------------------------------------------------------------------------
add interface function
called from dlil when a new interface is attached for that family
needs to set the frameout and demux dlil specific functions in the ifp
----------------------------------------------------------------------------- */
int ppp_fam_add_if(struct ifnet *ifp)
{
    struct ppp_fam	*pppfam;
    
    LOGDBG(ifp, (LOGVAL, "ppp_fam_add_if, ifp = 0x%x\n", ifp));
    
    MALLOC(pppfam, struct ppp_fam *, sizeof(struct ppp_fam), M_TEMP, M_WAIT);
    if (!pppfam) {
        LOGDBG(ifp, (LOGVAL, "ppp_fam_ifoutput : Can't allocate interface family structure\n"));
        return 1;
    }

    bzero(pppfam, sizeof(struct ppp_fam));
    
    ifp->family_cookie = (u_long)pppfam;
    ifp->if_framer = ppp_fam_frameout;
    ifp->if_demux  = ppp_fam_demux;
    
    return 0;
}

/* -----------------------------------------------------------------------------
delete interface function
called from dlil when an interface is detached for that family
----------------------------------------------------------------------------- */
int ppp_fam_del_if(struct ifnet *ifp)
{
    LOGDBG(ifp, (LOGVAL, "ppp_fam_del_if, ifp = 0x%x\n", ifp));

    if (ifp->family_cookie) {
        FREE(ifp->family_cookie, M_TEMP);
        ifp->family_cookie = 0;
    }

    return 0;
}

/* -----------------------------------------------------------------------------
add protocol function
called from dlil when a network protocol is attached for an
interface from that family (i.e ip is attached through ppp_attach_ip)
----------------------------------------------------------------------------- */
int  ppp_fam_add_proto(struct ddesc_head_str *desc_head, struct if_proto *proto, u_long dl_tag)
{
    struct ifnet		*ifp = proto->ifp;
    struct ppp_fam		*pppfam = (struct ppp_fam *)ifp->family_cookie;
    struct dlil_demux_desc	*desc = TAILQ_FIRST(desc_head);
    u_short			protocol = 0;
    
    if (desc)
        protocol = *((u_short *)desc->native_type);
    
    LOGDBG(ifp, (LOGVAL, "ppp_fam_add_proto = %d, ifp = 0x%x\n", protocol, ifp));
    
    switch (protocol) {
        case PPP_IP:
            if (pppfam->ip_tag)	// protocol already registered
                return EEXIST;
            pppfam->ip_tag = dl_tag;
            pppfam->ip_proto = proto;
            break;
        case PPP_IPV6:
            if (pppfam->ipv6_tag)	// protocol already registered
                return EEXIST;
            pppfam->ipv6_tag = dl_tag;
            pppfam->ipv6_proto = proto;
            break;
        default:
            return EINVAL;	// happen for unknown protocol, or for empty descriptor
    }

    return 0;
}

/* -----------------------------------------------------------------------------
delete protocol function
called from dlil when a network protocol is detached for an
interface from that family (i.e ip is attached through ppp_detach_ip)
----------------------------------------------------------------------------- */
int  ppp_fam_del_proto(struct if_proto *proto, u_long dl_tag)
{
    struct ifnet	*ifp = proto->ifp;
    struct ppp_fam  	*pppfam = (struct ppp_fam *)ifp->family_cookie;

    LOGDBG(ifp, (LOGVAL, "ppp_fam_del_proto, ifp = 0x%x\n", ifp));

    if (dl_tag == pppfam->ip_tag) {
        pppfam->ip_tag = 0;
        pppfam->ip_proto = 0;
    }

    if (dl_tag == pppfam->ipv6_tag) {
        pppfam->ipv6_tag = 0;
        pppfam->ipv6_proto = 0;
    }
    
    return 0;
}

/* -----------------------------------------------------------------------------
demux function
----------------------------------------------------------------------------- */
int ppp_fam_demux(struct ifnet *ifp, struct mbuf *m, char *frame_header,
                  void *if_proto_ptr)
{
    struct ppp_fam  	*pppfam = (struct ppp_fam *)ifp->family_cookie;
    u_int16_t 		proto;

    proto = frame_header[0];
    if (!proto & 0x1) {  // lowest bit set for lowest byte of protocol
        proto = (proto << 8) + frame_header[1];
    } 

    switch (proto) {
        case PPP_IP:
            *(struct if_proto **)if_proto_ptr = pppfam->ip_proto;
            break;
        case PPP_IPV6:
            *(struct if_proto **)if_proto_ptr = pppfam->ipv6_proto;
            break;
        default :
            LOGDBG(ifp, (LOGVAL, "ppp_fam_demux, ifp = 0x%x, bad proto = 0x%x\n", ifp, proto));
            return ENOENT;	// should never happen
    }
    
    return 0;
}

/* -----------------------------------------------------------------------------
a network packet needs to be send through the interface.
add the ppp header to the packet (as a network interface, we only worry
about adding our protocol number)
----------------------------------------------------------------------------- */
int ppp_fam_frameout(struct ifnet *ifp, struct mbuf **m0,
                     struct sockaddr *ndest, char *edst, char *ppp_type)
{

    M_PREPEND (*m0, 2, M_DONTWAIT);
    if (*m0 == 0) {
        LOGDBG(ifp, (LOGVAL, "ppp_fam_ifoutput : no memory for transmit header\n"));
        ifp->if_oerrors++;
        return EJUSTRETURN;	// just return, because the buffer was freed in m_prepend
    }

    // place protocol number at the beginning of the mbuf
    *mtod(*m0, u_int16_t *) = htons(*(u_int16_t *)ppp_type);
    
    return 0;
}

