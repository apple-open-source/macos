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

/* -----------------------------------------------------------------------------
 *
 *  History :
 *
 *  Jun 2000 - Christophe Allie - created.
 *
 *  Theory of operation :
 *
 *  This contains the family part of PPP.
 *  Handle LCP, PAP, CHAP, IPCP via connection to pppd
 *  and demux IP protocol
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

#include "ppp.h"
#include "ppp_fam.h"
#include "ppp_ip.h"
#include "ppp_domain.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

#define TYPE2FAMILY(subfam) 	((((u_long)subfam) << 16) + APPLE_IF_FAM_PPP)
#define FAMILY2TYPE(family) 	((family >> 16) & 0xFFFF)

struct ppp_desc {
    TAILQ_ENTRY(ppp_desc) next;
    u_short           	ppptype;
    u_long	    	dl_tag;
    struct ifnet	*ifp;
    struct if_proto 	*proto;
};

struct ppp_if {
    time_t		last_xmit; 	// last proto packet sent on this interface
    time_t		last_recv; 	// last proto packet received on this interface
    void 		*data;		// private data for ppp_domain
};

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

int  	ppp_fam_add_if(struct ifnet *ifp);
int 	ppp_fam_del_if(struct ifnet *ifp);
int  	ppp_fam_add_proto(struct ddesc_head_str *desc_head, struct if_proto *proto, u_long dl_tag);
int  	ppp_fam_del_proto(struct if_proto *proto, u_long dl_tag);
int 	ppp_fam_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data);

int 	ppp_fam_demux(struct ifnet *ifp, struct mbuf *m, char *frame_header,
                   void  *if_proto_ptr);
int 	ppp_fam_frameout(struct ifnet *ifp, struct mbuf **m0,
                      struct sockaddr *ndest, char *edst, char *ppp_type);
int 	ppp_fam_event(struct ifnet *ifp, caddr_t evp);

int 	ppp_fam_if_connecting(struct ifnet *ifp);
int 	ppp_fam_if_connected(struct ifnet *ifp);
int 	ppp_fam_if_disconnecting(struct ifnet *ifp);
int 	ppp_fam_if_disconnected(struct ifnet *ifp);

int 	ppp_fam_sendevent(struct ifnet *ifp, u_long code);

struct if_proto *dlttoproto(u_long dl_tag);
struct ifnet *ifbyfamily(u_long family, short unit);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

static TAILQ_HEAD(, ppp_desc) 	ppp_desc_head;


/* -----------------------------------------------------------------------------
init function
----------------------------------------------------------------------------- */
int ppp_fam_init()
{
    int 			ret;
    struct dlil_ifmod_reg_str   mod;

//    log(LOGVAL, "ppp_fam_init\n");

    TAILQ_INIT(&ppp_desc_head);

    // register the module
    bzero(&mod, sizeof(struct dlil_ifmod_reg_str));
    mod.add_if = ppp_fam_add_if;
    mod.del_if = ppp_fam_del_if;
    mod.add_proto = ppp_fam_add_proto;
    mod.del_proto = ppp_fam_del_proto;
    mod.ifmod_ioctl = ppp_fam_ioctl;
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

//    log(LOGVAL, "ppp_fam_dispose\n");

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
    struct ppp_if  	*pppif;
    //struct ifpppreq 	req;

    LOGDBG(ifp, (LOGVAL, "ppp_fam_add_if, ifp = 0x%x\n", ifp));

    MALLOC(pppif, struct ppp_if *, sizeof(struct ppp_if), M_DEVBUF, M_WAIT);
    if (!pppif) {
        log(LOGVAL, "ppp_fam_add_if : Can't allocate interface\n");
        return 1;
    }

    bzero(pppif, sizeof(struct ppp_if));

#if 0
    bzero(&req, sizeof (struct ifpppreq));
    req.ifr_code = IFPPP_CAPS;
    if (ifp->if_ioctl)
        (*ifp->if_ioctl)(ifp, SIOCGIFPPP, &req);

    pppif->framing =  = req.ifr_caps.framing;
#endif
    
    ifp->family_cookie = (u_long)pppif;
    ifp->if_framer = ppp_fam_frameout;
    ifp->if_demux  = ppp_fam_demux;
    ifp->if_event = ppp_fam_event;
    
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
        FREE(ifp->family_cookie, M_DEVBUF);
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
    struct dlil_demux_desc  	*desc;
    struct ppp_desc  		*pppdesc;

    LOGDBG(proto->ifp, (LOGVAL, "ppp_fam_add_proto, ifp = 0x%x\n", proto->ifp));

    TAILQ_FOREACH(desc, desc_head, next) {

        if (desc->type != DLIL_DESC_RAW)
            return EINVAL;

        MALLOC(pppdesc, struct ppp_desc *, sizeof(struct ppp_desc), M_DEVBUF, M_WAIT);
        if (!pppdesc) {
            log(LOGVAL, "ppp_fam_add_proto : Can't allocate descriptor\n");
            return ENOMEM;
        }

        bzero(pppdesc, sizeof(struct ppp_desc));

        pppdesc->ppptype = *((unsigned short *) desc->native_type);
        pppdesc->dl_tag = dl_tag;
        pppdesc->proto = proto;
        pppdesc->ifp = proto->ifp;

        TAILQ_INSERT_TAIL(&ppp_desc_head, pppdesc, next);
    }

    return 0;
}

/* -----------------------------------------------------------------------------
add more demux descriptor for that protocol
----------------------------------------------------------------------------- */
int  ppp_fam_add_protodesc(struct ddesc_head_str *desc_head, u_long dl_tag)
{

    ppp_fam_add_proto(desc_head, dlttoproto(dl_tag), dl_tag);
    return 0;
}

/* -----------------------------------------------------------------------------
delete protocol function
called from dlil when a network protocol is detached for an
interface from that family (i.e ip is attached through ppp_detach_ip)
----------------------------------------------------------------------------- */
int  ppp_fam_del_proto(struct if_proto *proto, u_long dl_tag)
{
    struct ppp_desc  	*curdesc, *nextdesc;

    LOGDBG(proto->ifp, (LOGVAL, "ppp_fam_del_proto, ifp = 0x%x\n", proto->ifp));

    // remove all the associated descriptor
    nextdesc = TAILQ_FIRST(&ppp_desc_head);
    while (nextdesc) {
        curdesc = nextdesc;
        nextdesc = TAILQ_NEXT(nextdesc, next);
        if (curdesc->dl_tag == dl_tag) {
            TAILQ_REMOVE(&ppp_desc_head, curdesc, next);
            FREE(curdesc, M_DEVBUF);
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
delete the demux structure for that type
----------------------------------------------------------------------------- */
int  ppp_fam_del_protodesc(u_short ppptype, u_long dl_tag)
{
    struct ppp_desc  	*curdesc, *nextdesc;

   nextdesc = TAILQ_FIRST(&ppp_desc_head);
    while (nextdesc) {
        curdesc = nextdesc;
        nextdesc = TAILQ_NEXT(nextdesc, next);
        if ((curdesc->dl_tag == dl_tag)
            && (curdesc->ppptype == ppptype)) {
          TAILQ_REMOVE(&ppp_desc_head, curdesc, next);
            FREE(curdesc, M_DEVBUF);
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------------------------------------------------------
Process an ioctl request to the ppp network interface
------------------------------------------------------------------------------------------------------------------------------------------ */
int ppp_fam_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
    struct proc 	*p = current_proc();	/* XXX */
    struct ifpppreq 	*ifpppr = (struct ifpppreq *)data;
    int 		s = splimp(), error = 0;
    time_t		t;
    struct ppp_if	*pppif = (struct ppp_if *)ifp->family_cookie;

  //LOGDBG(ifp, (LOGVAL, "ppp_if_ioctl: cmd = 0x%x\n", cmd));

    switch (cmd) {

        case SIOCSIFPPP:
            if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
                return error;
            switch (ifpppr->ifr_code) {
                case IFPPP_EFLAGS:
                    ifp->if_eflags |= (ifpppr->ifr_eflags & IFF_ELINKMASK);
                    LOGDBG(ifp, (LOGVAL, "ppp_fam_ioctl: cmd = SIOCSIFPPP/IFPPP_EFLAGS, flags = 0x%x\n", ifp->if_eflags));
                    break;
                case IFPPP_ATTACH_IP:
                    LOGDBG(ifp, (LOGVAL, "ppp_fam_ioctl: cmd = SIOCSIFPPP/IFPPP_ATTACHIP\n"));
                    ppp_ip_attach(ifp, data);
                    break;
                case IFPPP_DETACH_IP:
                    LOGDBG(ifp, (LOGVAL, "ppp_fam_ioctl: cmd = SIOCSIFPPP/IFPPP_DETACHIP\n"));
                    ppp_ip_detach(ifp);
                    break;
           }
            break;
        case SIOCGIFPPP:
            switch (ifpppr->ifr_code) {
                case IFPPP_EFLAGS:
                    ifpppr->ifr_eflags = ifp->if_eflags & IFF_ELINKMASK;
                    LOGDBG(ifp, (LOGVAL, "ppp_fam_ioctl: cmd = SIOCGIFPPP/IFPPP_EFLAGS, flags = 0x%x\n", ifp->if_eflags));
                   break;
                case IFPPP_IDLE:
                    t = time_second;
                    if (pppif) {
                        ifpppr->ifr_idle.xmit_idle = t - pppif->last_xmit;
                        ifpppr->ifr_idle.recv_idle = t - pppif->last_recv;
                    }
                    break;
                case IFPPP_STATS:		
                    bzero(&ifpppr->ifr_stats, sizeof(struct ppp_stats));
                    ifpppr->ifr_stats.ibytes = ifp->if_ibytes;
                    ifpppr->ifr_stats.obytes = ifp->if_obytes;
                    ifpppr->ifr_stats.ipackets = ifp->if_ipackets;
                    ifpppr->ifr_stats.opackets = ifp->if_opackets;
                    ifpppr->ifr_stats.ierrors = ifp->if_ierrors;
                    ifpppr->ifr_stats.oerrors = ifp->if_oerrors;
        #if defined(VJC) && !defined(SL_NO_STATS)
                    if (sc->sc_comp) {
                        psp->vj.vjs_packets = sc->sc_comp->sls_packets;
                        psp->vj.vjs_compressed = sc->sc_comp->sls_compressed;
                        psp->vj.vjs_searches = sc->sc_comp->sls_searches;
                        psp->vj.vjs_misses = sc->sc_comp->sls_misses;
                        psp->vj.vjs_uncompressedin = sc->sc_comp->sls_uncompressedin;
                        psp->vj.vjs_compressedin = sc->sc_comp->sls_compressedin;
                        psp->vj.vjs_errorin = sc->sc_comp->sls_errorin;
                        psp->vj.vjs_tossed = sc->sc_comp->sls_tossed;
                    }
        #endif /* VJC */
                        break;
            }

            break;
            

#if PPP_COMPRESS
        case SIOCGPPPCSTATS:
            ppp_comp_getstats(sc, &((struct ifpppcstatsreq *) data)->stats);
            break;
#endif /* PPP_COMPRESS */

        default:
            error = EOPNOTSUPP;
    }
    splx(s);
    return (error);
}

/* -----------------------------------------------------------------------------
demux function
called from dlil when a packet from the interface needs to be dispatched.
the demux function for this family only consists to call the offer function for the
first protocol attached to the family.
ppplink is designed in such a way that only the demux needs to attach to the if
the interface gives packet to the mux as-is, the mux will reconstruct the complete
ppp header based on the received data and on
the acceptable flags for address/control and protocol compression
the interface can also analyze as well the incoming packets and determine if is
is correct (async ppp)
we handle here two special (temporary?) events
----------------------------------------------------------------------------- */
int ppp_fam_demux(struct ifnet *ifp, struct mbuf *m, char *frame_header,
                  void *if_proto_ptr)
{
    u_short 		proto, len;
    u_char 		*p;
    struct ppp_if  	*pppif = (struct ppp_if *)ifp->family_cookie;
    struct ppp_desc 	*desc;

    // need to analyze the header here
    // can check with LCP negociation or could be flexible :-)
    p = mtod(m, u_char *);
    len = 0;
    if (p[len] == PPP_ALLSTATIONS) {
        len++;
        if (p[len] == PPP_UI)
            len++;
    }
    proto = p[len++];
    if (!(proto & 0x1)) {  // lowest bit set for lowest byte of protocol
        proto = (proto << 8) + p[len++];
    }

    // we now have a ppptype which looks like a good ppp type
    // Search through the connected protocols for a match.

    TAILQ_FOREACH(desc, &ppp_desc_head, next) {
        if ((desc->ifp == ifp)
            && (desc->ppptype == proto)) {

            if (len < 2) {
                // prepend the a full 2 bytes protocol (can't use frame header for that)
                // frame header is something used by the driver, not by the family
                // in out case, the driver does not dicriminate header/data
                // because it depends on the ppp/lcp negociation
                M_PREPEND (m, 2 - len, M_DONTWAIT);
                if (m == 0) {
                    log(LOGVAL, "ppp_fam_input : no memory for prepend header\n");
                    return EJUSTRETURN;
                }
            }
            if (len > 2) {
                // remove the extra bytes, so that we have space for only a complete protocol
                m_adj(m, len - 2);
            }
            p = mtod(m, u_char *);
            *(u_short *)p = proto;
            *(struct if_proto **)if_proto_ptr = desc->proto;

            // set now the mbuf to point to the actual data
            // protocol modules can retrive the 2 bytes header by looking in front of the data
            m_adj(m, 2);
            
            /* special hack to handle the TCP/IP VJ from the family code  */
            if (proto == PPP_VJC_COMP || proto == PPP_VJC_UNCOMP) {
                if (ppp_ip_processvj(m, ifp, desc->dl_tag)) {
                    return EJUSTRETURN;
                }
            }
            /* */
            
            pppif->last_recv = time_second;
            return 0;
        }
    }

    // prepend the header because pppd expects it
    M_PREPEND (m, sizeof(struct ppp_header) - len, M_DONTWAIT);
    if (m == 0) {
        log(LOGVAL, "ppp_fam_input : no memory for transmit header\n");
        return EJUSTRETURN;
    }

    p = mtod(m, u_char *);
    p[0] = PPP_ALLSTATIONS;
    p[1] = PPP_UI;
    *(u_short *)(p + 2) = proto;

    // if this interface is controlled, then send the data
    if (pppif->data) {
        ppp_proto_input(FAMILY2TYPE(ifp->if_family), ifp->if_unit, pppif->data, m);
        return EJUSTRETURN;
    }
    
    return ENOENT;
}

/* -----------------------------------------------------------------------------
frameout function
a network packet needs to be send through the interface.
add the ppp header to the packet (as a network interface, we only worry
about adding our protocol number)
----------------------------------------------------------------------------- */
int ppp_fam_frameout(struct ifnet *ifp, struct mbuf **m0,
                     struct sockaddr *ndest, char *edst, char *ppp_type)
{
    u_char 		*p;
    u_short		len, i;
    struct ppp_if	*pppif = (struct ppp_if *)ifp->family_cookie;


    //if (!(ifp->if_flags & IFF_UP)) {
    if (!(ifp->if_flags & IFF_PPP_CONNECTED)) {
        p = mtod(*m0, u_char *);
        log(LOGVAL, "--------- DIAL ON DEMAND TRIGGERED -------, 0x ");
        len = (*m0)->m_pkthdr.len;
        if (len > 80) len = 80;	// just log the beginning
        for (i = 0; i < len; i++) 
            log(LOGVAL, "%x ", p[i]);
        log(LOGVAL, "\n");
        // need to bufferize the packet
        m_freem(*m0);
        ifp->if_oerrors++;
        ppp_fam_sendevent(ifp, KEV_PPP_NEEDCONNECT);
        return EJUSTRETURN;	// just return, because the buffer was freed
    }

    /* special hack to handle the TCP/IP VJ from the family code  */
    if (*(u_short *)ppp_type == PPP_IP) {
        if (ppp_ip_processvj_out(m0, ifp, ppp_type)) {
            return 1; // humm...
        }
    }
    /* */

    //log(LOGVAL, "ppp_fam_ifoutput, 0x %x %x %x %x %x %x %x %x\n", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);

    // first try to remove the first protocol byte
    len = 1;
    if (ppp_type[0] || !(ifp->if_eflags & IFF_PPP_DOES_PCOMP))
        len++;

    M_PREPEND (*m0, len, M_DONTWAIT);
    if (*m0 == 0) {
        log(LOGVAL, "ppp_fam_ifoutput : no memory for transmit header\n");
        ifp->if_oerrors++;
        return EJUSTRETURN;	// just return, because the buffer was freed in m_prepend
    }

    p = mtod(*m0, u_char *);
    if (ppp_type[0] || !(ifp->if_eflags & IFF_PPP_DOES_PCOMP))
        *p++ = ppp_type[0];
    *p = ppp_type[1];
    

    // then prepend address/control only if necessary
    if (!(ifp->if_eflags & (IFF_PPP_DOES_ACCOMP + IFF_PPP_DEL_AC))) {
        M_PREPEND (*m0, 2, M_DONTWAIT);
        if (*m0 == 0) {
            log(LOGVAL, "ppp_fam_ifoutput : no memory for transmit header\n");
            ifp->if_oerrors++;
            return EJUSTRETURN;	// just return, because the buffer was freed in m_prepend
        }
        p = mtod(*m0, u_char *);
        p[0] = PPP_ALLSTATIONS;
        p[1] = PPP_UI;
    }

    pppif->last_xmit = time_second;
 
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_fam_event(struct ifnet *ifp, caddr_t evp)
{
    struct kern_event_msg *evt = (struct kern_event_msg *)evp;
    int 	ret = 0;
        
    if ((evt->vendor_code != KEV_VENDOR_APPLE)
        || (evt->kev_class != KEV_NETWORK_CLASS)
        || (evt->kev_subclass != KEV_PPP_SUBCLASS)) {

        log(LOGVAL, "ppp_fam_event : event does not belong to PPP\n");
        return 0;
    }

    switch (evt->event_code) {
        case KEV_PPP_PACKET_LOST:
            LOGDBG(ifp, (LOGVAL, "ppp_fam_event : PACKET_LOST\n"));
            ret = 0;
            break;

        case KEV_PPP_CONNECTING:
            LOGDBG(ifp, (LOGVAL, "ppp_fam_event : EVT_CONNECTING\n"));
            ppp_fam_if_connecting(ifp);
            break;

        case KEV_PPP_CONNECTED:
            LOGDBG(ifp, (LOGVAL, "ppp_fam_event : EVT_CONNECTED\n"));
            ppp_fam_if_connected(ifp);
            break;

        case KEV_PPP_DISCONNECTING:
            LOGDBG(ifp, (LOGVAL, "ppp_fam_event : EVT_DISCONNECTING\n"));
            ppp_fam_if_disconnecting(ifp);
            break;

        case KEV_PPP_DISCONNECTED:
            LOGDBG(ifp, (LOGVAL, "ppp_fam_event : EVT_DISCONNECTED\n"));
            ppp_fam_if_disconnected(ifp);
            break;

        case KEV_PPP_LISTENING:
            LOGDBG(ifp, (LOGVAL, "ppp_fam_event : EVT_LISTENING\n"));
            //ppp_fam_if_connecting(ifp);
            break;

        case KEV_PPP_RINGING:
            LOGDBG(ifp, (LOGVAL, "ppp_fam_event : EVT_RINGING\n"));
           //ppp_fam_if_connecting(ifp);
            break;

        case KEV_PPP_ACCEPTING:
            LOGDBG(ifp, (LOGVAL, "ppp_fam_event : EVT_ACCEPTING\n"));
            //ppp_fam_if_connecting(ifp);
            break;

        default:
            //log(LOGVAL, "ppp_fam_event : unknown PPP CLASS event %ld\n", evt->event_code);
            ppp_fam_if_disconnected(ifp);
            break;
}

    return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_fam_if_connecting(struct ifnet *ifp)
{
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_fam_if_connected(struct ifnet *ifp)
{
    struct ppp_if  	*pppif = (struct ppp_if *)ifp->family_cookie;

    pppif->last_xmit = pppif->last_recv = time_second;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_fam_if_disconnecting(struct ifnet *ifp)
{
    //ppp_ip_detach(ifp); // temporary, until we receive SIOCDIFADDR
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_fam_if_disconnected(struct ifnet *ifp)
{
    //ppp_ip_detach(ifp); // temporary, until we receive SIOCDIFADDR
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_fam_sockregister(u_short subfam, u_short unit, void *data)
{
    struct ifnet 	*ifp;

    if (ifp = ifbyfamily(TYPE2FAMILY(subfam), unit))
        (*(struct ppp_if *)(ifp->family_cookie)).data = data;

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_fam_sockderegister(u_short subfam, u_short unit)
{
    struct ifnet 	*ifp;
    //struct ifpppreq    	req;

    if (ifp = ifbyfamily(TYPE2FAMILY(subfam), unit)) {

        (*(struct ppp_if *)(ifp->family_cookie)).data = 0;

        // just disconnect the interface, in case the controller forgot...
        //req.ifr_code = IFPPP_DISCONNECT;
        //dlil_ioctl(0, ifp, SIOCSIFPPP, (caddr_t)&req);
    }

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_fam_sockoutput(u_short subfam, u_short unit, struct mbuf *m)
{
    struct ifnet 	*ifp;

    if (ifp = ifbyfamily(TYPE2FAMILY(subfam) , unit)) {
        
        /* calling if_output directly is not correct, because filters are not called
        but there is no function in dlil api to call the output function if
        we are neither a protocol, nor a filter */
        if (ifp->if_eflags & IFF_PPP_DEL_AC) {
            m_adj(m, 2); // remove the proto header (frame header is not used)
        }
        return (*(ifp)->if_output)(ifp, m);
    }

    return 1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_fam_sendevent(struct ifnet *ifp, u_long code)
{
    struct kev_ppp_msg		evt;

    bzero(&evt, sizeof(struct kev_ppp_msg));
    evt.total_size 	= KEV_MSG_HEADER_SIZE + sizeof(struct kev_ppp_data);
    evt.vendor_code 	= KEV_VENDOR_APPLE;
    evt.kev_class 	= KEV_NETWORK_CLASS;
    evt.kev_subclass 	= KEV_PPP_SUBCLASS;
    evt.id 		= 0;
    evt.event_code 	= code;
    evt.event_data.link_data.if_family = ifp->if_family;
    evt.event_data.link_data.if_unit = ifp->if_unit;
    bcopy(ifp->if_name, &evt.event_data.link_data.if_name[0], IFNAMSIZ);

    dlil_event(ifp, (struct kern_event_msg *)&evt);
    return 0;
}

