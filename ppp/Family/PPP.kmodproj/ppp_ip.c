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


/* ------------------------------------------------------------------------------------------------------------------------------------------
Includes
------------------------------------------------------------------------------------------------------------------------------------------ */

#include <sys/types.h>
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

#include "ppp.h"
#include "ppp_fam.h"
#include "ppp_ip.h"
#include <net/slcompress.h>


/* ------------------------------------------------------------------------------------------------------------------------------------------
Definitions
------------------------------------------------------------------------------------------------------------------------------------------ */

struct ppp_ip {
    TAILQ_ENTRY(ppp_ip) next;
    unsigned long 	dl_tag;
    struct ifnet 	*ifp;
    struct slcompress	*vjcomp; 		/* vjc control buffer */
    u_char 		vjcid; 
};

/* ------------------------------------------------------------------------------------------------------------------------------------------
Forward declarations
------------------------------------------------------------------------------------------------------------------------------------------ */

static int ppp_ip_input(struct mbuf *m, char *frame_header, struct ifnet *ifp,
                        u_long dl_tag, int sync_ok);
static int ppp_ip_preoutput(struct ifnet *ifp, struct mbuf **m0, struct sockaddr *dst_netaddr,
                            caddr_t route, char *type, char *edst, u_long dl_tag );
static int ppp_ip_event(struct kern_event_msg  *evt, u_long dl_tag);
static int ppp_ip_ioctl(u_long dl_tag, struct ifnet *ifp, u_long cmd, caddr_t data);

/* ------------------------------------------------------------------------------------------------------------------------------------------
PPP globals
------------------------------------------------------------------------------------------------------------------------------------------ */

static TAILQ_HEAD(, ppp_ip) 	ppp_ip_head;

/* ------------------------------------------------------------------------------------------------------------------------------------------
init function
------------------------------------------------------------------------------------------------------------------------------------------ */
int ppp_ip_init(int init_arg)
{

//    log(LOGVAL, "ppp_ip_init\n");
    TAILQ_INIT(&ppp_ip_head);
    return 0;
}

/* ------------------------------------------------------------------------------------------------------------------------------------------
terminate function
------------------------------------------------------------------------------------------------------------------------------------------ */
int ppp_ip_dispose(int term_arg)
{

//    log(LOGVAL, "ppp_ip_dispose\n");
    return 0;
}

/* ------------------------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------------------------ */
struct ppp_ip *ppp_ip_findbytag(u_long dl_tag)
{
    struct ppp_ip	*ipcp;
    
    TAILQ_FOREACH(ipcp, &ppp_ip_head, next)
        if (ipcp->dl_tag == dl_tag)
            return ipcp;

    return 0;
}

/* ------------------------------------------------------------------------------------------------------------------------------------------
attach the PPPx interface ifp to the network protocol IP,
called when the ppp interface is ready for ppp traffic
------------------------------------------------------------------------------------------------------------------------------------------ */
int ppp_ip_attach(struct ifnet *ifp, u_char *data)
{
    int 			ret;
    struct dlil_proto_reg_str   reg;
    struct dlil_demux_desc      desc;
    struct ppp_ip	      	*ipcp;
    u_short	      		ipproto = PPP_IP;
    
    LOGDBG(ifp, (LOGVAL, "ppp_ip_attach: name = %s, unit = %d\n", ifp->if_name, ifp->if_unit));

    TAILQ_FOREACH(ipcp, &ppp_ip_head, next)
        if (ipcp->ifp == ifp) 
            return 0;	// already attached

    MALLOC(ipcp, struct ppp_ip *, sizeof(struct ppp_ip), M_DEVBUF, M_WAIT);
    if (!ipcp) {
        log(LOGVAL, "ppp_ip_attach : Can't allocate ppp_ip structure\n");
        return ENOMEM;
    }

    bzero(ipcp, sizeof(struct ppp_ip));
    ipcp->ifp = ifp;

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
    reg.pre_output       = ppp_ip_preoutput;
    reg.event            = ppp_ip_event;
    reg.ioctl		 = ppp_ip_ioctl;
    ret = dlil_attach_protocol(&reg, &ipcp->dl_tag);
    if (ret) {
        FREE(ipcp, M_DEVBUF);
        LOGRETURN(ret, ret, "ppp_attach_ip: dlil_attach_protocol error = 0x%x\n");
    }

    TAILQ_INSERT_TAIL(&ppp_ip_head, ipcp, next);
    LOGDBG(ifp, (LOGVAL, "ppp_attach_ip: dlil_attach_protocol tag = 0x%x\n", ipcp->dl_tag));

    return 0;
}

/* ------------------------------------------------------------------------------------------------------------------------------------------
detach the PPPx interface ifp from the network protocol IP,
called when the ppp interface stops ip traffic
------------------------------------------------------------------------------------------------------------------------------------------ */
int ppp_ip_detach(struct ifnet *ifp)
{
    int 		ret;
    unsigned long	dl_tag;
    struct ppp_ip  	*curdesc, *nextdesc;

    LOGDBG(ifp, (LOGVAL, "ppp_ip_detach\n"));

    if (!dlil_find_dltag(ifp->if_family, ifp->if_unit, PF_INET, &dl_tag)) {
        ret = dlil_detach_protocol(dl_tag);

        nextdesc = TAILQ_FIRST(&ppp_ip_head);
        while (nextdesc) {
            curdesc = nextdesc;
            nextdesc = TAILQ_NEXT(nextdesc, next);
            if (curdesc->dl_tag == dl_tag) {
                if (curdesc->vjcomp) 
                    FREE(curdesc->vjcomp, M_DEVBUF);
                TAILQ_REMOVE(&ppp_ip_head, curdesc, next);
                FREE(curdesc, M_DEVBUF);
                break;
            }
        }

        LOGRETURN(ret, ret, "ppp_ip_detach: dlil_detach_protocol error = 0x%x\n");
    }

    return 0;
}

/* ------------------------------------------------------------------------------------------------------------------------------------------
input function
called from dlil when a packet from the interface is to be dispatched to
the specific network protocol attached by dl_tag.
the network protocol has been determined earlier by the demux function.
the packet is in the mbuf chain m without
the frame header, which is provided separately. (not used)
------------------------------------------------------------------------------------------------------------------------------------------ */
int ppp_ip_input(struct mbuf *m, char *frame_header, struct ifnet *ifp,
                 u_long dl_tag, int sync_ok)
{
    struct ppp_ip 	*ipcp = ppp_ip_findbytag(dl_tag);
    int 		inlen, s, vjlen;
    u_char		*p, *iphdr;
    u_short 		proto;
    struct mbuf 	*mp;
    u_int 		hlen;

    LOGMBUF("ppp_ip_input (in)", m);

    if (!ipcp)
        return 1; // humm, should not happen... probably very bad

    p = mtod(m, u_char *);
    inlen = m->m_pkthdr.len;

    // a 2 bytes protocol field has been placed by the family in front of the packet
    proto = *(u_short *)(p - 2);

    switch (proto) {

        case PPP_VJC_COMP:

            vjlen = sl_uncompress_tcp_core(p, m->m_len, inlen, TYPE_COMPRESSED_TCP,
                                           ipcp->vjcomp, &iphdr, &hlen);

            if (vjlen <= 0) {
                LOGDBG(ifp, (LOGVAL, "ppp%d: VJ uncompress failed on type comp\n", ifp->if_unit));
                return EIO; // ???
            }

            /* Copy the PPP and IP headers into a new mbuf. */
            MGETHDR(mp, M_DONTWAIT, MT_DATA);
            if (mp == NULL)
                return 1;
            mp->m_len = 0;
            mp->m_next = NULL;
            if (hlen > MHLEN) {
                MCLGET(mp, M_DONTWAIT);
                if (M_TRAILINGSPACE(mp) < hlen) {
                    m_freem(mp);
                    return ENOMEM;	/* lose if big headers and no clusters */
                }
            }
            p = mtod(mp, u_char *);
            bcopy(iphdr, p, hlen);
            mp->m_len = hlen;

            /*
             * VJ headers off the old mbuf
             * and stick the new and old mbufs together.
             */
            m->m_data += vjlen;
            m->m_len -= vjlen;
            if (m->m_len <= M_TRAILINGSPACE(mp)) {
                bcopy(mtod(m, u_char *), mtod(mp, u_char *) + mp->m_len, m->m_len);
                mp->m_len += m->m_len;
                MFREE(m, mp->m_next);
            } else
                mp->m_next = m;
            m = mp;
            inlen += hlen - vjlen;

            m->m_pkthdr.len = inlen;  // adjust the new packet len
            m->m_pkthdr.rcvif = ifp;	
            break;

        case PPP_VJC_UNCOMP:

            vjlen = sl_uncompress_tcp_core(p, m->m_len, inlen, TYPE_UNCOMPRESSED_TCP,
                                           ipcp->vjcomp, &iphdr, &hlen);

            if (vjlen < 0) {
                LOGDBG(ifp, (LOGVAL, "ppp%d: VJ uncompress failed on type uncomp\n", ifp->if_unit));
                return EIO; // ???
            }

           break;
    }


    if (ipflow_fastforward(m)) {
        //sc->sc_last_recv = time_second;
        return 0;
    }

    schednetisr(NETISR_IP);
    //sc->sc_last_recv = time_second;	/* update time of last pkt rcvd */

    /* Put the packet on the ip input queue */
    s = splimp();
    if (IF_QFULL(&ipintrq)) {
        IF_DROP(&ipintrq);
        splx(s);
        //if (sc->sc_flags & SC_DEBUG)
        //    printf("ppp%d: input queue full\n", ifp->if_unit);
        ifp->if_iqdrops++;
        return 1;
    }
    IF_ENQUEUE(&ipintrq, m);
    splx(s);

    LOGMBUF("ppp_ip_input (out)", m);
    return 0;
}

/* ------------------------------------------------------------------------------------------------------------------------------------------
special function handling the VJ compression from the ppp family demultiplexer code.
we do this ugly hack because the Shared IP protocol filter expect to find an IP packet...
Shared IP filter needs to be moved one layer up, at the ip_input level, but we don't have filtering facilities at this level yet.
------------------------------------------------------------------------------------------------------------------------------------------ */
int ppp_ip_processvj(struct mbuf *m, struct ifnet *ifp, u_long dl_tag)
{
    struct ppp_ip 	*ipcp = ppp_ip_findbytag(dl_tag);
    int 		inlen, vjlen;
    u_char		*p, *iphdr;
    u_short 		proto;
    u_int 		hlen;

    LOGMBUF("ppp_ip_processvj (in)", m);

    if (!ipcp) {
        m_free(m);
        return 1; // humm, should not happen... probably very bad
    }

    p = mtod(m, u_char *);
    inlen = m->m_pkthdr.len;

    // a 2 bytes protocol field has been placed by the family in front of the packet
    proto = *(u_short *)(p - 2);

    switch (proto) {

        case PPP_VJC_COMP:

            vjlen = sl_uncompress_tcp_core(p, m->m_len, inlen, TYPE_COMPRESSED_TCP,
                                           ipcp->vjcomp, &iphdr, &hlen);

            if (vjlen <= 0) {
                LOGDBG(ifp, (LOGVAL, "ppp%d: VJ uncompress failed on type comp\n", ifp->if_unit));
                m_free(m);
                return 1; 
            }
            // we must move data in the buffer, to add the uncompressed TCP/IP header...
            if (M_TRAILINGSPACE(m) < (hlen - vjlen)) {
                m_free(m);
                return 1; 
            }
            bcopy(p + vjlen, p + hlen, inlen - vjlen);
            bcopy(iphdr, p, hlen);
            m->m_len += hlen - vjlen;
            m->m_pkthdr.len += hlen - vjlen;
            break;

        case PPP_VJC_UNCOMP:

            vjlen = sl_uncompress_tcp_core(p, m->m_len, inlen, TYPE_UNCOMPRESSED_TCP,
                                           ipcp->vjcomp, &iphdr, &hlen);

            if (vjlen < 0) {
                LOGDBG(ifp, (LOGVAL, "ppp%d: VJ uncompress failed on type uncomp\n", ifp->if_unit));
                m_free(m);
                return 1; // ???
            }

           break;
    }

    *(u_short *)(p - 2) = PPP_IP;
    LOGMBUF("ppp_ip_processvj (out)", m);
    return 0;
}

/* -----------------------------------------------------------------------------
pre_output function
----------------------------------------------------------------------------- */
int ppp_ip_preoutput(struct ifnet *ifp, struct mbuf **m0, struct sockaddr *dst_netaddr,
                     caddr_t route, char *type, char *edst, u_long dl_tag)
{
    struct ppp_ip 	*ipcp = ppp_ip_findbytag(dl_tag);
//    int			vjtype;
    struct ip 		*ip;
    u_short 		proto = PPP_IP;

    //log(LOG_INFO, "preoutput ip\n");
    LOGMBUF("ppp_ip_preoutput", *m0);

    if (!ipcp)
        return 1; // humm, should not happen... probably very bad

    (*m0)->m_flags &= ~M_HIGHPRI;

    /*
     If this packet has the "low delay" bit set in the IP header,
     put it on the fastq instead.
     */
    ip = mtod(*m0, struct ip *);
    if (ip->ip_tos & IPTOS_LOWDELAY)
        (*m0)->m_flags |= M_HIGHPRI;

    /*
     If the packet is a TCP/IP packet, see if we can compress it.
     this code assumes the IP/TCP header is in one non-shared mbuf
     */
#if 0
/* remove it as special hack to handle the TCP/IP VJ from the family code  */
    if (ip->ip_p == IPPROTO_TCP && ipcp->vjcomp) {
        vjtype = sl_compress_tcp(*m0, ip, ipcp->vjcomp, ipcp->vjcid);
        switch (vjtype) {
            case TYPE_UNCOMPRESSED_TCP:
                proto = PPP_VJC_UNCOMP;
                break;
            case TYPE_COMPRESSED_TCP:
                proto = PPP_VJC_COMP;
                break;
        }
    }
#endif

    *(u_short *)type = proto;
    return 0;
}

/* -----------------------------------------------------------------------------
pre_output function
----------------------------------------------------------------------------- */
int ppp_ip_processvj_out(struct mbuf **m0, struct ifnet *ifp, char *type)
{
    struct ppp_ip 	*ipcp = 0;
    int			vjtype;
    struct ip 		*ip;
    u_short 		proto = PPP_IP;
    
    TAILQ_FOREACH(ipcp, &ppp_ip_head, next)
        if (ipcp->ifp == ifp)
            break;

    if (!ipcp)
        return 1; // humm, should not happen... probably very bad

    ip = mtod(*m0, struct ip *);

    /*
     If the packet is a TCP/IP packet, see if we can compress it.
     this code assumes the IP/TCP header is in one non-shared mbuf
     */
    if (ip->ip_p == IPPROTO_TCP && ipcp->vjcomp) {
        vjtype = sl_compress_tcp(*m0, ip, ipcp->vjcomp, ipcp->vjcid);
        switch (vjtype) {
            case TYPE_UNCOMPRESSED_TCP:
                proto = PPP_VJC_UNCOMP;
                break;
            case TYPE_COMPRESSED_TCP:
                proto = PPP_VJC_COMP;
                break;
        }
    }

    *(u_short *)type = proto;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_ip_ioctl(u_long dl_tag, struct ifnet *ifp, u_long cmd, caddr_t data)
{
    struct proc 		*p = current_proc();	/* XXX */
    struct ifpppreq 		*ifpppr = (struct ifpppreq *)data;
    struct in_ifaddr 		*ia = (struct in_ifaddr *)data;
    struct ppp_ip 		*ipcp = ppp_ip_findbytag(dl_tag);
    struct ddesc_head_str	desc_head;
    struct dlil_demux_desc      desc, desc2;
    u_short	      		vjcproto = PPP_VJC_COMP;
    u_short	      		vjuproto = PPP_VJC_UNCOMP;
    int 			error = 0;

    //LOGDBG(ifp, (LOGVAL, "ppp_ip_ioctl\n"));

    switch (cmd) {
        case SIOCSIFADDR:
        case SIOCAIFADDR:
            LOGDBG(ifp, (LOGVAL, "ppp_ip_ioctl: cmd = SIOCSIFADDR/SIOCAIFADDR\n"));
            // plumb now the ip protocol previously attached
            ia->ia_ifa.ifa_dlt = ipcp->dl_tag;
            break;

        case SIOCDIFADDR:
            LOGDBG(ifp, (LOGVAL, "ppp_ip_ioctl: cmd = SIOCDIFADDR\n"));
            break;

        case SIOCSIFDSTADDR:
            LOGDBG(ifp, (LOGVAL, "ppp_ip_ioctl: cmd = SIOCSIFDSTADDR\n"));
            break;
            
        case SIOCSIFPPP:
        LOGDBG(ifp, (LOGVAL, "ppp_ip_ioctl: cmd = IFPPP_IP_VJ: name = %s, unit = %d, vj = %d, cid = %d, maxcid = %d\n", ifp->if_name, ifp->if_unit, ifpppr->ifr_ip_vj.vj, ifpppr->ifr_ip_vj.cid, ifpppr->ifr_ip_vj.max_cid));
            if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
                return error;
            switch (ifpppr->ifr_code) {
                case IFPPP_IP_VJ:

                    LOGDBG(ifp, (LOGVAL, "ppp_ip_ioctl: cmd = IFPPP_IP_VJ: name = %s, unit = %d, vj = %d, cid = %d, maxcid = %d\n", ifp->if_name, ifp->if_unit, ifpppr->ifr_ip_vj.vj, ifpppr->ifr_ip_vj.cid, ifpppr->ifr_ip_vj.max_cid));

                    ipcp->vjcid = ifpppr->ifr_ip_vj.cid;
                    if (ifpppr->ifr_ip_vj.vj) {
                        if (!ipcp->vjcomp)
                            MALLOC(ipcp->vjcomp, struct slcompress *, sizeof(struct slcompress),
                                   M_DEVBUF, M_NOWAIT);

                        if (ipcp->vjcomp) {

                            sl_compress_init(ipcp->vjcomp, ifpppr->ifr_ip_vj.max_cid > 2 ? ifpppr->ifr_ip_vj.max_cid : -1);

                           // add vj descriptor...
                            TAILQ_INIT(&desc_head);
                            bzero(&desc, sizeof(struct dlil_demux_desc));
                            desc.type = DLIL_DESC_RAW;
                            desc.native_type = (char *) &vjcproto;
                            TAILQ_INSERT_TAIL(&desc_head, &desc, next);
                            desc2 = desc;
                            desc2.native_type = (char *) &vjuproto;
                            TAILQ_INSERT_TAIL(&desc_head, &desc2, next);
                            ppp_fam_add_protodesc(&desc_head, ipcp->dl_tag);
                        }
                    }
                    else {
                        if (ipcp->vjcomp) {
                            FREE(ipcp->vjcomp, M_DEVBUF);
                            ipcp->vjcomp = 0;
                        }

                        // del vj descriptor...
                        ppp_fam_del_protodesc(PPP_VJC_COMP, ipcp->dl_tag);
                        ppp_fam_del_protodesc(PPP_VJC_UNCOMP, ipcp->dl_tag);
                    }
                    break;
            }
            break;

    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_ip_event(struct kern_event_msg  *evt, u_long dl_tag)
{
    struct ppp_ip 	*ipcp = ppp_ip_findbytag(dl_tag);
    int 		ret = 0;
    
    if (!ipcp)
        return 1; // humm, should not happen... probably very bad

    LOGDBG(ipcp->ifp, (LOGVAL, "ppp_ip_event\n"));

    if ((evt->vendor_code != KEV_VENDOR_APPLE)
        || (evt->kev_class != KEV_NETWORK_CLASS)
        || (evt->kev_subclass != KEV_PPP_SUBCLASS))
        return 0; // hum??? interfaces should not generate that event ???

    switch (evt->event_code) {
        case KEV_PPP_PACKET_LOST:
            LOGDBG(ipcp->ifp, (LOGVAL, "ppp_ip_event : PACKET_LOST\n"));
            if (ipcp->vjcomp)
                sl_uncompress_tcp(NULL, 0, TYPE_ERROR, ipcp->vjcomp);
           // ret = EJUSTRETURN; ???
            break;
    }

    return ret;
}


