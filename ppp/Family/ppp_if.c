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
*  this file implements the interface driver for the ppp family
*
*  it's the responsability of the driver to update the statistics
*  whenever that makes sense
*     ifnet.if_lastchange = a packet is present, a packet starts to be sent
*     ifnet.if_ibytes = nb of correct PPP bytes received (does not include escapes...)
*     ifnet.if_obytes = nb of correct PPP bytes sent (does not include escapes...)
*     ifnet.if_ipackets = nb of PPP packet received
*     ifnet.if_opackets = nb of PPP packet sent
*     ifnet.if_ierrors = nb on input packets in error
*     ifnet.if_oerrors = nb on ouptut packets in error
*
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Includes
----------------------------------------------------------------------------- */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <machine/spl.h>
#include <kern/clock.h>

#include <net/if_types.h>
#include <net/dlil.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <net/bpf.h>

#include "slcompress.h"
#include "ppp_defs.h"		// public ppp values
#include "if_ppp.h"		// public ppp API
#include "if_ppplink.h"		// public link API
#include "ppp_if.h"
#include "ppp_domain.h"
#include "ppp_ip.h"
#include "ppp_ipv6.h"
#include "ppp_compress.h"
#include "ppp_comp.h"
#include "ppp_link.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

static int	ppp_if_output(struct ifnet *ifp, struct mbuf *m);
static int	ppp_if_if_free(struct ifnet *ifp);
static int 	ppp_if_ioctl(struct ifnet *ifp, u_long cmd, void *data);

static int 	ppp_if_detach(struct ifnet *ifp);
static struct ppp_if *ppp_if_findunit(u_short unit);
static u_short 	ppp_if_findfreeunit();

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

static TAILQ_HEAD(, ppp_if) 	ppp_if_head;

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_if_init()
{
    TAILQ_INIT(&ppp_if_head);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_if_dispose()
{

    // can't dispose if interface are in use
    if (!TAILQ_EMPTY(&ppp_if_head))
        return EBUSY;
        
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_if_attach(u_short *unit)
{
    int 		ret = 0;	
    struct ppp_if  	*wan;
    struct ifnet 	*ifp;

    MALLOC(wan, struct ppp_if *, sizeof(struct ppp_if), M_TEMP, M_WAITOK);
    if (!wan)
        return ENOMEM;

    bzero(wan, sizeof(struct ppp_if));

    ret = dlil_if_acquire(APPLE_IF_FAM_PPP, 0, 0, &wan->net);
    if (ret)
        goto error;

    // check if number requested is already in use
    if ((*unit != 0xFFFF) && ppp_if_findunit(*unit)) {
        ret = EINVAL;
        goto error;
    }

    // it's time now to register our brand new channel
    ifp = wan->net;
    ifp->if_softc 	= wan;
    ifp->if_name 	= APPLE_PPP_NAME;
    ifp->if_family 	= APPLE_IF_FAM_PPP;
    ifp->if_mtu 	= PPP_MTU;
    ifp->if_flags 	= IFF_POINTOPOINT | IFF_MULTICAST; // || IFF_RUNNING
    ifp->if_type 	= IFT_PPP;
    ifp->if_hdrlen 	= PPP_HDRLEN;
    ifp->if_ioctl 	= ppp_if_ioctl;
    ifp->if_output 	= ppp_if_output;
    ifp->if_free 	= ppp_if_if_free;
    ifp->if_baudrate 	= 0; // 10 Mbits/s ???
    ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
    getmicrotime(&ifp->if_lastchange);
    ifp->if_ibytes = ifp->if_obytes = 0;
    ifp->if_ipackets = ifp->if_opackets = 0;
    ifp->if_ierrors = ifp->if_oerrors = 0;

    TAILQ_INIT(&wan->link_head);

    ifp->if_unit = *unit != 0xFFFF ? *unit : ppp_if_findfreeunit();
    *unit = ifp->if_unit;
    ret = dlil_if_attach(ifp);
    if (ret)
        goto error;
    
    TAILQ_INSERT_TAIL(&ppp_if_head, wan, next);
    bpfattach(ifp, DLT_PPP, PPP_HDRLEN);

    // attach network protocols
    wan->npmode[NP_IP] = NPMODE_ERROR;
    wan->npmode[NP_IPV6] = NPMODE_ERROR;

    return 0;

error:
    if (wan->net)
        dlil_if_release(wan->net);
    FREE(wan, M_TEMP);
    return ret;
}

/* -----------------------------------------------------------------------------
detach ppp interface from dlil layer
----------------------------------------------------------------------------- */
int ppp_if_detach(struct ifnet *ifp)
{
    struct ppp_if  	*wan = (struct ppp_if *)ifp->if_softc;
    int 		ret;
    struct ppp_link	*link;
    struct mbuf		*m;

    // need to remove all ref to ifnet in link structures
    TAILQ_FOREACH(link, &wan->link_head, lk_bdl_next) {
        // do we need a free function ?
        link->lk_ifnet = 0;
    }

    ppp_comp_close(wan);

    // detach protocols when detaching interface, just in case pppd forgot... 
    ppp_ipv6_detach(ifp, 0 /* not used */);

    ppp_ip_detach(ifp, 0 /* not used */);	
    if (wan->vjcomp) {
	FREE(wan->vjcomp, M_TEMP);
	wan->vjcomp = 0;
    }

    ret = dlil_if_detach(ifp);
    switch (ret) {
        case 0:
            break;
        case DLIL_WAIT_FOR_FREE:
            sleep(ifp, PZERO+1);
            break;
        default:
            return KERN_FAILURE;
    }

    do {
        IF_DEQUEUE(&ifp->if_snd, m);
        m_freem(m);
    } while (m);

    ifp->if_softc = 0;
    dlil_if_release(ifp);
    TAILQ_REMOVE(&ppp_if_head, wan, next);
    FREE(wan, M_TEMP);

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_if_attachclient(u_short unit, void *host, struct ifnet **ifp)
{
    struct ppp_if  	*wan;

    wan = ppp_if_findunit(unit);
    if (!wan)
        return ENODEV;

    *ifp = wan->net;
    if (!wan->host)    // don't override the first attachment (use a list ?)
        wan->host = host;
    wan->nbclients++;   
    
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_if_detachclient(struct ifnet *ifp, void *host)
{
    struct ppp_if  	*wan = (struct ppp_if *)ifp->if_softc;

    if (wan->host) {
        if (wan->host == host)
            wan->host = 0;
        wan->nbclients--;
        if (!wan->nbclients)
            ppp_if_detach(ifp);
    }
}

/* -----------------------------------------------------------------------------
find a free unit in the interface list
----------------------------------------------------------------------------- */
u_short ppp_if_findfreeunit()
{
    struct ppp_if  	*wan = TAILQ_FIRST(&ppp_if_head);
    u_short 		unit = 0;

    while (wan) {
    	if (wan->net->if_unit == unit) {
            unit++;
            wan = TAILQ_FIRST(&ppp_if_head); // restart
        }
        else 
            wan = TAILQ_NEXT(wan, next); // continue
    }
    return unit;
}

/* -----------------------------------------------------------------------------
find a the unit number in the interface list
----------------------------------------------------------------------------- */
struct ppp_if *ppp_if_findunit(u_short unit)
{
    struct ppp_if  	*wan;

    TAILQ_FOREACH(wan, &ppp_if_head, next) {
        if (wan->net->if_unit == unit)
            return wan; 
    }
    return NULL;
}

/* -----------------------------------------------------------------------------
called when data are present
----------------------------------------------------------------------------- */
int ppp_if_input(struct ifnet *ifp, struct mbuf *m, u_int16_t proto, u_int16_t hdrlen)
{    
    struct ppp_if 	*wan = (struct ppp_if *)ifp->if_softc;
    int 		inlen, vjlen;
    u_char		*iphdr, *p = mtod(m, u_char *);
    u_int 		hlen;
    int 		error = ENOMEM;

    m->m_pkthdr.header = p;		// header point to the protocol header (0x21 or 0x0021)
    m_adj(m, hdrlen);			// the packet points to the real data (0x45)
    p = mtod(m, u_char *);

    if (wan->sc_flags & SC_DECOMP_RUN) {
        switch (proto) {
            case PPP_COMP:
                if (ppp_comp_decompress(wan, &m) != DECOMP_OK) {
                    LOGDBG(ifp, (LOGVAL, "ppp%d: decompression error\n", ifp->if_unit));
                    goto free;
                }
                p = mtod(m, u_char *);
                proto = p[0];
                hdrlen = 1;
                if (!(proto & 0x1)) {  // lowest bit set for lowest byte of protocol
                    proto = (proto << 8) + p[1];
                    hdrlen = 2;
                } 
                m->m_pkthdr.header = p;// header point to the protocol header (0x21 or 0x0021)
                m_adj(m, hdrlen);	// the packet points to the real data (0x45)
                p = mtod(m, u_char *);
                break;
            default:
                ppp_comp_incompress(wan, m);
        }
    }

    switch (proto) {
        case PPP_VJC_COMP:
        case PPP_VJC_UNCOMP:
            
            if (!(wan->sc_flags & SC_COMP_TCP))
                goto reject;
        
            if (!wan->vjcomp) {
                LOGDBG(ifp, (LOGVAL, "ppp%d: VJ structure not allocated\n", ifp->if_unit));
                goto free;
            }
    
            inlen = m->m_pkthdr.len;
            
            if (proto == PPP_VJC_COMP) {

                vjlen = sl_uncompress_tcp_core(p, m->m_len, inlen, TYPE_COMPRESSED_TCP,
                    wan->vjcomp, &iphdr, &hlen);

                if (vjlen <= 0) {
                    LOGDBG(ifp, (LOGVAL, "ppp%d: VJ uncompress failed on type PPP_VJC_COMP\n", ifp->if_unit));
                    goto free;
                }

                // we must move data in the buffer, to add the uncompressed TCP/IP header...
                if (M_TRAILINGSPACE(m) < (hlen - vjlen)) {
                    goto free;
                }
                bcopy(p + vjlen, p + hlen, inlen - vjlen);
                bcopy(iphdr, p, hlen);
                m->m_len += hlen - vjlen;
                m->m_pkthdr.len += hlen - vjlen;
            }
            else {
                vjlen = sl_uncompress_tcp_core(p, m->m_len, inlen, TYPE_UNCOMPRESSED_TCP, 
                    wan->vjcomp, &iphdr, &hlen);

                if (vjlen < 0) {
                    LOGDBG(ifp, (LOGVAL, "ppp%d: VJ uncompress failed on type TYPE_UNCOMPRESSED_TCP\n", ifp->if_unit));
                    goto free;
                }
            }
            *(u_char *)m->m_pkthdr.header = PPP_IP; // change the protocol, use 1 byte
            proto = PPP_IP;
            //no break;
        case PPP_IP:
            if (wan->npmode[NP_IP] != NPMODE_PASS)
                goto reject;
            if (wan->npafmode[NP_IP] & NPAFMODE_SRC_IN) {
                if (ppp_ip_af_src_in(ifp, mtod(m, char *))) {
                    error = 0;
                    goto free;
                }
            }
            break;
        case PPP_IPV6:
            if (wan->npmode[NP_IPV6] != NPMODE_PASS)
                goto reject;
            break;
        case PPP_CCP:
            ppp_comp_ccp(wan, m, 1);
            goto reject;
        default:
            goto reject;
    }

    // See if bpf wants to look at the packet.
    if (ifp->if_bpf) {
        M_PREPEND(m, 4, M_WAIT);
        if (m == 0) {
            ifp->if_ierrors++;
            return ENOMEM;
        }
        p = mtod(m, u_char *);
        *(u_int16_t *)p = 0xFF03;
        *(u_int16_t *)(p + 2) = proto;
        bpf_mtap(ifp, m);
        m_adj(m, 4);
    }

    getmicrotime(&ifp->if_lastchange);
    ifp->if_ibytes += m->m_pkthdr.len;
    ifp->if_ipackets++;
    m->m_pkthdr.rcvif = ifp;
    wan->last_recv = clock_get_system_value().tv_sec;

    dlil_input(ifp, m, m);
    return 0;
    
reject:

    // unexpected network protocol
    M_PREPEND(m, hdrlen, M_DONTWAIT);	// just reput the header before to send it to pppd
    ppp_proto_input(wan->host, m);
    return 0;
    
free:
    m_free(m);
    ifp->if_ierrors++;
    return error;
}

/* -----------------------------------------------------------------------------
This gets called when the interface is freed
(if dlil_if_detach has returned DLIL_WAIT_FOR_FREE)
----------------------------------------------------------------------------- */
int ppp_if_if_free(struct ifnet *ifp)
{
    wakeup(ifp);
    return 0;
}

/* -----------------------------------------------------------------------------
Process an ioctl request to the ppp interface
----------------------------------------------------------------------------- */
int ppp_if_control(struct ifnet *ifp, u_long cmd, void *data)
{
    struct ppp_if 	*wan = (struct ppp_if *)ifp->if_softc;
    int 		error = 0, npx;
    u_int16_t		mru;
    u_int32_t		flags;
    u_int32_t		t;
    struct npioctl 	*npi;
    struct npafioctl 	*npafi;

    //LOGDBG(ifp, (LOGVAL, "ppp_if_control, (ifnet = %s%d), cmd = 0x%x\n", ifp->if_name, ifp->if_unit, cmd));

    switch (cmd) {
	case PPPIOCSDEBUG:
            flags = *(int *)data;
            LOGDBG(ifp, (LOGVAL, "ppp_if_control: PPPIOCSDEBUG (level = 0x%x)\n", flags));
            ifp->if_flags &= ~(IFF_DEBUG + PPP_LOG_INPKT + PPP_LOG_OUTPKT);  
            if (flags & 1) ifp->if_flags |= IFF_DEBUG;	// general purpose debugging
            if (flags & 2) ifp->if_flags |= PPP_LOG_INPKT;	// trace all packets in
            if (flags & 4) ifp->if_flags |= PPP_LOG_OUTPKT;	// trace all packets out
            break;

	case PPPIOCGDEBUG:
            flags = 0;
            if (ifp->if_flags & IFF_DEBUG) flags |= 1;
            if (ifp->if_flags & PPP_LOG_INPKT) flags |= 2;
            if (ifp->if_flags & PPP_LOG_OUTPKT) flags |= 4;
            *(int *)data = flags;
            LOGDBG(ifp, (LOGVAL, "ppp_if_control: PPPIOCGDEBUG (level = 0x%x)\n", flags));
            break;

	case PPPIOCSMRU:
            LOGDBG(ifp, (LOGVAL, "ppp_if_control: PPPIOCSMRU\n"));
            mru = *(int *)data;
            wan->mru = mru;
            break;

	case PPPIOCSFLAGS:
            flags = *(int *)data & SC_MASK;
            LOGDBG(ifp, (LOGVAL, "ppp_if_control: PPPIOCSFLAGS, old flags = 0x%x new flags = 0x%x, \n", wan->sc_flags, (wan->sc_flags & ~SC_MASK) | flags));
            wan->sc_flags = (wan->sc_flags & ~SC_MASK) | flags;
            break;

	case PPPIOCGFLAGS:
            LOGDBG(ifp, (LOGVAL, "ppp_if_control: PPPIOCGFLAGS\n"));
            *(int *)data = wan->sc_flags;
            break;

	case PPPIOCSCOMPRESS:
            error = ppp_comp_setcompressor(wan, data);
            break;

	case PPPIOCGUNIT:
            LOGDBG(ifp, (LOGVAL, "ppp_if_control: PPPIOCGUNIT\n"));
            *(int *)data = ifp->if_unit;
            break;

	case PPPIOCGIDLE:
            LOGDBG(ifp, (LOGVAL, "ppp_if_control: PPPIOCGIDLE\n"));
            t = clock_get_system_value().tv_sec;
            ((struct ppp_idle *)data)->xmit_idle = t - wan->last_xmit;
            ((struct ppp_idle *)data)->recv_idle = t - wan->last_recv;
            break;

        case PPPIOCSMAXCID:
            LOGDBG(ifp, (LOGVAL, "ppp_if_control: PPPIOCSMAXCID\n"));
            // allocate the vj structure first
            if (!wan->vjcomp) {
                MALLOC(wan->vjcomp, struct slcompress *, sizeof(struct slcompress), 
                    M_TEMP, M_WAITOK); 	
                if (!wan->vjcomp) 
                    return ENOMEM;
                sl_compress_init(wan->vjcomp, -1);
            }
            // reeinit the compressor
            sl_compress_init(wan->vjcomp, *(int *)data);
            break;

	case PPPIOCSNPMODE:
	case PPPIOCGNPMODE:
            LOGDBG(ifp, (LOGVAL, "ppp_if_control: PPPIOCSNPMODE/PPPIOCGNPMODE\n"));
            npi = (struct npioctl *) data;
            switch (npi->protocol) {
                case PPP_IP:
                    npx = NP_IP;
                    break;
                case PPP_IPV6:
                    npx = NP_IPV6;
                   break;
                default:
                    return EINVAL;
            }
            if (cmd == PPPIOCGNPMODE) {
                npi->mode = wan->npmode[npx];
            } else {                
                if (npi->mode != wan->npmode[npx]) {
                    wan->npmode[npx] = npi->mode;
                    if (npi->mode != NPMODE_QUEUE) {
                        //ppp_requeue(sc);
                        //(*sc->sc_start)(sc);
                    }
                }
            }
            break;

	case PPPIOCSNPAFMODE:
	case PPPIOCGNPAFMODE:
            LOGDBG(ifp, (LOGVAL, "ppp_if_control: PPPIOCSNPAFMODE/PPPIOCGNPAFMODE\n"));
            npafi = (struct npafioctl *) data;
            switch (npafi->protocol) {
                case PPP_IP:
                    npx = NP_IP;
                    break;
                case PPP_IPV6:
                    npx = NP_IPV6;
                    break;
                default:
                    return EINVAL;
            }
            if (cmd == PPPIOCGNPMODE) {
                npafi->mode = wan->npafmode[npx];
            } else {          
                wan->npafmode[npx] = npafi->mode;
            }
            break;

	default:
            LOGDBG(ifp, (LOGVAL, "ppp_if_control: unknown ioctl\n"));
            error = EINVAL;
	}

    return error;
}

/* -----------------------------------------------------------------------------
Process an ioctl request to the ppp interface
----------------------------------------------------------------------------- */
int ppp_if_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
    //struct ppp_if 	*wan = (struct ppp_if *)ifp->if_softc;
    struct ifreq 	*ifr = (struct ifreq *)data;
    int 		error = 0;
    struct ppp_stats 	*psp;

    //LOGDBG(ifp, (LOGVAL, "ppp_if_ioctl, cmd = 0x%x\n", cmd));

    switch (cmd) {

        case SIOCSIFFLAGS:
            LOGDBG(ifp, (LOGVAL, "ppp_if_ioctl: cmd = SIOCSIFFLAGS\n"));
            // even if this case does nothing, it must be there to return 0
            //if ((ifp->if_flags & IFF_RUNNING) == 0)
            //    ifp->if_flags &= ~IFF_UP;
            break;

        case SIOCSIFADDR:
        case SIOCAIFADDR:
            LOGDBG(ifp, (LOGVAL, "ppp_if_ioctl: cmd = SIOCSIFADDR/SIOCAIFADDR\n"));
            // dlil protocol module already took care of the ioctl
            break;

        case SIOCADDMULTI:
        case SIOCDELMULTI:
            LOGDBG(ifp, (LOGVAL, "ppp_if_ioctl: cmd = SIOCADDMULTI/SIOCDELMULTI\n"));
            break;

        case SIOCDIFADDR:
            LOGDBG(ifp, (LOGVAL, "ppp_if_ioctl: cmd = SIOCDIFADDR\n"));
            break;

        case SIOCSIFDSTADDR:
            LOGDBG(ifp, (LOGVAL, "ppp_if_ioctl: cmd = SIOCSIFDSTADDR\n"));
            break;

	case SIOCGPPPSTATS:
            LOGDBG(ifp, (LOGVAL, "ppp_if_ioctl, SIOCGPPPSTATS\n"));
            psp = &((struct ifpppstatsreq *) data)->stats;
            bzero(psp, sizeof(*psp));

            psp->p.ppp_ibytes = ifp->if_ibytes;
            psp->p.ppp_obytes = ifp->if_obytes;
            psp->p.ppp_ipackets = ifp->if_ipackets;
            psp->p.ppp_opackets = ifp->if_opackets;
            psp->p.ppp_ierrors = ifp->if_ierrors;
            psp->p.ppp_oerrors = ifp->if_oerrors;

#if 0
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
#endif
            break;

        case SIOCSIFMTU:
            LOGDBG(ifp, (LOGVAL, "ppp_if_ioctl, SIOCSIFMTU\n"));
            // should we check the minimum MTU for all channels attached to that interface ?
            if (ifr->ifr_mtu > PPP_MTU)
                error = EINVAL;
            else
                ifp->if_mtu = ifr->ifr_mtu;
            break;

	default:
            LOGDBG(ifp, (LOGVAL, "ppp_if_ioctl, unknown ioctl, cmd = 0x%x\n", cmd));
            error = EOPNOTSUPP;
	}

    return error;
}

/* -----------------------------------------------------------------------------
This gets called at splnet from if_ppp.c at various times
when there is data ready to be sent
----------------------------------------------------------------------------- */
int ppp_if_output(struct ifnet *ifp, struct mbuf *m)
{
    struct ppp_if 	*wan = (struct ppp_if *)ifp->if_softc;
    int 		error;
    u_int16_t		proto;
    enum NPmode		mode;
    enum NPAFmode	afmode;
    char		*p;
    
    proto = ntohs(*mtod(m, u_short *));
    switch (proto) {
        case PPP_IP:
            mode = wan->npmode[NP_IP];
            afmode = wan->npafmode[NP_IP];
            break;
        case PPP_IPV6:
            mode = wan->npmode[NP_IPV6];
            afmode = wan->npafmode[NP_IPV6];
            break;
        default:
            // should never happen, since we attached the protocol ourself
            error = EAFNOSUPPORT;
            goto bad;
    }

    switch (mode) {
        case NPMODE_ERROR:
            error = ENETDOWN;
            goto bad;
        case NPMODE_QUEUE:
        case NPMODE_DROP:
            error = 0;
            goto bad;
        case NPMODE_PASS:
            break;
    }
    
    if (afmode & NPAFMODE_SRC_OUT) {
        p = mtod(m, char *);
        p += 2;
        switch (proto) {
            case PPP_IP:
                error = ppp_ip_af_src_out(ifp, p);
                break;
        }
        if (error) {
            error = 0;
            goto bad;
        }
    }

    // See if bpf wants to look at the packet.
    if (ifp->if_bpf) {
        M_PREPEND(m, 2, M_WAIT);
        if (m == 0) {
            ifp->if_oerrors++;
            return ENOMEM;
        }
        *mtod(m, u_int16_t *) = 0xFF03;
        bpf_mtap(ifp, m);
        m_adj(m, 2);
    }

    // Update interface statistics.
    getmicrotime(&ifp->if_lastchange);
    wan->last_xmit = clock_get_system_value().tv_sec;
    ifp->if_obytes += m->m_pkthdr.len - 2; // don't count protocol header
    ifp->if_opackets++;

    if (wan->sc_flags & SC_LOOP_TRAFFIC) {
        ppp_proto_input(wan->host, m);
        return 0;
    }
        
    ppp_if_send(ifp, m);
    return 0;

bad:
    m_freem(m);
    ifp->if_oerrors++;
    return error;
}

/* -----------------------------------------------------------------------------
 * Connect a PPP channel to a PPP interface unit.
----------------------------------------------------------------------------- */
int ppp_if_attachlink(struct ppp_link *link, int unit)
{
    struct ppp_if 	*wan;

    if (link->lk_ifnet)	// already attached
        return EINVAL;

    wan = ppp_if_findunit(unit);
    if (!wan)
	return EINVAL;

    wan->net->if_flags |= IFF_RUNNING;
    wan->net->if_baudrate += link->lk_baudrate;

    TAILQ_INSERT_TAIL(&wan->link_head, link, lk_bdl_next);
    wan->nblinks++;
    link->lk_ifnet = wan->net;

    return 0;
}

/* -----------------------------------------------------------------------------
 * Disconnect a channel from its ppp unit.
----------------------------------------------------------------------------- */
int ppp_if_detachlink(struct ppp_link *link)
{
    struct ifnet 	*ifp = (struct ifnet *)link->lk_ifnet;
    struct ppp_if 	*wan;

    if (!ifp)
        return EINVAL; // link already detached

    wan = (struct ppp_if *)ifp->if_softc;
    
    // check if this is the last link, when multilink is coded
    ifp->if_flags &= ~IFF_RUNNING;
    ifp->if_baudrate -= link->lk_baudrate;
    
    TAILQ_REMOVE(&wan->link_head, link, lk_bdl_next);
    wan->nblinks--;
    link->lk_ifnet = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_if_send(struct ifnet *ifp, struct mbuf *m)
{
    struct ppp_if 	*wan = (struct ppp_if *)ifp->if_softc;
    u_int16_t		proto = *mtod(m, u_int16_t *);	// always the 2 first bytes
        
    if (IF_QFULL(&ifp->if_snd)) {
        IF_DROP(&ifp->if_snd);
        ifp->if_oerrors++;
        m_freem(m);
        return ENOBUFS;
    }

    switch (proto) {
        case PPP_IP:
            // see if we can compress it
            if ((wan->sc_flags & SC_COMP_TCP) && wan->vjcomp) {
                struct mbuf 	*mp = m;
                struct ip 	*ip = (struct ip *) (mtod(m, u_char *) + 2);
                int 		vjtype, len;
                
                // skip mbuf, in case the ppp header and ip header are not in the same mbuf
                if (mp->m_len <= 2) {
                    mp = mp->m_next;
                    if (!mp)
                        break;
                    ip = mtod(mp, struct ip *);
                }
                // this code assumes the IP/TCP header is in one non-shared mbuf 
                if (ip->ip_p == IPPROTO_TCP) {
                    vjtype = sl_compress_tcp(mp, ip, wan->vjcomp, !(wan->sc_flags & SC_NO_TCP_CCID));
                    switch (vjtype) {
                        case TYPE_UNCOMPRESSED_TCP:
                            *mtod(m, u_int16_t *) = PPP_VJC_UNCOMP; // update protocol
                            break;
                        case TYPE_COMPRESSED_TCP:
                            *mtod(m, u_int16_t *) = PPP_VJC_COMP; // header has moved, update protocol
                        break;
                    }
                    // adjust packet len
                    len = 0;
                    for (mp = m; mp != 0; mp = mp->m_next)
                        len += mp->m_len;
                    m->m_pkthdr.len = len;
                }
            }
            break;
        case PPP_CCP:
            m_adj(m, 2);
            ppp_comp_ccp(wan, m, 0);
            M_PREPEND(m, 2, M_DONTWAIT);
            break;
    }

    if (wan->sc_flags & SC_COMP_RUN) {

        if (ppp_comp_compress(wan, &m) == COMP_OK) {
            M_PREPEND(m, 2, M_DONTWAIT);
            if (m == 0) {
                ifp->if_oerrors++;
                return ENOBUFS;
            }
            *mtod(m, u_int16_t *) = PPP_COMP; // update protocol
        } 
    } 

    if (ifp->if_snd.ifq_len) {
        IF_ENQUEUE(&ifp->if_snd, m);
    }
    else 
       ppp_if_xmit(ifp, m);
    
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_if_xmit(struct ifnet *ifp, struct mbuf *m)
{
    struct ppp_if 	*wan = (struct ppp_if *)ifp->if_softc;
    struct ppp_link	*link;
    int 		err, len;
            
    if (m == 0)
        IF_DEQUEUE(&ifp->if_snd, m);

    while (m) {

        link = TAILQ_FIRST(&wan->link_head);
        if (link == 0) {
            LOGDBG(ifp, (LOGVAL, "ppp%d: Trying to send data with link detached\n", ifp->if_unit));
            // just flush everything
            getmicrotime(&ifp->if_lastchange);
            while (m) {
                ifp->if_oerrors++;
                m_freem(m);
                IF_DEQUEUE(&ifp->if_snd, m);
            };
            return 1;
        }
    
        if (link->lk_flags & SC_HOLD) {
            // should try next link
            m_freem(m);
            IF_DEQUEUE(&ifp->if_snd, m);
            continue;
        }

        if (link->lk_flags & (SC_XMIT_BUSY | SC_XMIT_FULL)) {
            // should try next link
            IF_PREPEND(&ifp->if_snd, m);
            return 0;
        }

        // get the len before we send the packet, 
        // we can not assume the state of the mbuf when we return
        len = m->m_pkthdr.len;

        // since we tested the lk_flags, ppp_link_send should not failed
        // except if there is a dramatic error
        link->lk_flags |= SC_XMIT_BUSY;
        err = ppp_link_send(link, m);
        link->lk_flags &= ~SC_XMIT_BUSY;
        if (err) {
            // packet has been freed by link lower layer
            return err;
        }
            
        IF_DEQUEUE(&ifp->if_snd, m);
    }
     
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_if_error(struct ifnet *ifp)
{
    struct ppp_if 	*wan = (struct ppp_if *)ifp->if_softc;
        
    // reset vj compression
    if (wan->vjcomp) {
	sl_uncompress_tcp(NULL, 0, TYPE_ERROR, wan->vjcomp);
    }
}
