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
/* Copyright (c) 1997-2000 Apple Computer, Inc. All Rights Reserved */
/*
 * The Darwin (Apple Public Source) license specifies the terms
 * and conditions for redistribution.
 *
 * Shared IP address support for Classic/OT
 * Supports networking from Classic:
 *  for AppleTalk, OT and X have separate stacks and addresses
 *  for IP, OT and X have separate stacks, but share the same IP address(es)
 * This is the NKE that makes it all happen.
 *
 * Justin Walker, 991005
 * Laurent Dumont, 991112
 * 000824: Adding support for the PPP device type (IP only)
 */

/*
 * Theory of operation:
 * - init hook just registers
 * - kernel code finds this (find_nke()) when client executes
 *   SO_NKE setsockopt().
 * - invokes 'create' hook, to set up plumbing
 * - SO_PROTO_REGISTER ioctl induces filter registration, depending
 *   on filter definitions
 * - Output from client is filtered with socket NKE; output from X
 *   is filtered by data link protocol output filters
 * - Input from chosen devices is filtered by data link protocol
 *   input filters
 */
#include <sys/kdebug.h>
#if KDEBUG

#define DBG_SPLT_BFCHK  DRVDBG_CODE(DBG_DRVSPLT, 0)
#define DBG_SPLT_APPND  DRVDBG_CODE(DBG_DRVSPLT, 1)
#define DBG_SPLT_MBUF   DRVDBG_CODE(DBG_DRVSPLT, 2)
#define DBG_SPLT_DUP    DRVDBG_CODE(DBG_DRVSPLT, 3)
#define DBG_SPLT_PAD    DRVDBG_CODE(DBG_DRVSPLT, 4)

#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <net/route.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/ndrv.h>
#include <net/kext_net.h>
#include <net/dlil.h>
#include <netinet/in.h>		/* Needed for (*&^%$#@ arpcom in if_arp.h */
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <machine/spl.h>
#include <kern/thread.h>

#include "SharedIP.h"
#include "sip.h"

#include <sys/syslog.h>

/* hooks for PF_NKE usage */
int 	sif_connect(),
	sif_read(), sif_write(),
	sif_get(), sif_put();
void	sif_disconnect();

static int si_initted = 0;

int si_accept(), si_bind(), si_close(), si_connect(), si_control(),
    si_discon(), si_free(), si_receive(), si_send(), si_shutdown(),
    si_create();
int si_soisconnected();
int si_sbappend();

struct mbuf * m_dup(struct mbuf *, int);
int enable_filters(struct socket *, int, void *, struct kextcb *);

/* Dispatch vector for SharedIP socket intercepts */
struct sockif SIsockif = {
    NULL,		/* soabort */
	//	si_accept,	/* soaccept */
	NULL,			/* soaccept */
	si_bind,	/* sobind */
	si_close,	/* soclose */
	//	si_connect,	/* soconnect */
	NULL,			/* soconnect */
	NULL,		/* soconnect2 */
	si_control,	/* soset/getopt */
	si_create,	/* socreate */
	si_discon,	/* sodisconnect */
	//	si_free,	/* sofree */
	NULL,			/* sofree */
	NULL,		/* sogetopt */
	NULL,		/* sohasoutofband */
	NULL,		/* solisten */
	NULL,		/* soreceive */
	NULL,		/* sorflush */
	si_send,	/* sosend */
	NULL,		/* sosetopt */
	si_shutdown,	/* soshutdown */
	NULL,		/* socantrcvmore */
	NULL,		/* socantsendmore */
	//	si_soisconnected,/* soisconnected */
	NULL,			/* soisconnected */
	NULL,		/* soisconnecting */
	NULL,		/* soisdisconnected */
	NULL,		/* soisdisconnecting */
	NULL,		/* sonewconn1 */
	NULL,		/* soqinsque */
	NULL,		/* soqremque */
	NULL,		/* soreserve */
	NULL,		/* sowakeup */
};

/* Dispatch vector for SharedIP socket buffer functions */
struct sockutil SIsockutil = {
    NULL, /* sb_lock */
	si_sbappend, /* sbappend */
	NULL, /* sbappendaddr */
	NULL, /* sbappendcontrol */
	NULL, /* sbappendrecord */
	NULL, /* sbcompress */
	NULL, /* sbdrop */
	NULL, /* sbdroprecord */
	NULL, /* sbflush */
	NULL, /* sbinsertoob */
	NULL, /* sbrelease */
	NULL, /* sbreserve */
	NULL, /* sbwait */
};

struct NFDescriptor me = {
    {NULL, NULL},
	{NULL, NULL},
	SharedIP_Handle,
	NFD_PROG|NFD_VISIBLE,	/* Ask for me by name */
//	NFD_GLOBAL|NFS_VISIBLE,	/* only if we want global filtering */
	sif_connect,
	sif_disconnect,
	sif_read,
	sif_write,
	sif_get,
	sif_put,
	&SIsockif, &SIsockutil
};

struct blueCtlBlock *sipList;	/* Chain of all active control blocks */

int
SIP_start()
{
    struct protosw *pp;
    int s;
    int	funnel_state;

    funnel_state = thread_funnel_set(network_flock, TRUE);
    s = splnet();

    if ((pp = pffindproto(PF_NDRV, 0, SOCK_RAW)) == NULL) {
#if SIP_DEBUG_ERR
        log(LOG_WARNING, "Can't find PF_NDRV");
#endif
        splx(s);
        thread_funnel_set(network_flock, funnel_state);
        return(KERN_FAILURE);
    }
    if (register_sockfilter(&me, NULL, pp, NFF_BEFORE)) {
#if SIP_DEBUG_ERR
        log(LOG_WARNING, "Can't register SharedIP support");
#endif
        splx(s);
        thread_funnel_set(network_flock, funnel_state);
        return(KERN_FAILURE);
    }
    splx(s);

#if SIP_DEBUG
    log(LOG_WARNING, "SIP_start called successfully\n");
#endif
    si_initted = 1;
    thread_funnel_set(network_flock, funnel_state);
    return(KERN_SUCCESS);		/* Now, the waiting begins */
}

int
si_control(struct socket *so, struct sockopt *sopt, struct kextcb *kp)
{
    register int retval, s;
    struct sopt_proto_register proto_register;
    struct sopt_shared_port_param port_param;

    switch(sopt->sopt_name) {
        case SO_PROTO_REGISTER: {
#if SIP_DEBUG
            log(LOG_WARNING, "si_control: received SO_PROTO_REGISTER\n");
#endif

            retval = sooptcopyin(sopt, &proto_register, sizeof (struct sopt_proto_register),
                                    sizeof (proto_register));
            if (retval)
                return(retval);
            retval = enable_filters(so, proto_register.reg_flags,
                                    proto_register.reg_sa, kp);
            return(retval);
            break;
        }

        case SO_PORT_RESERVE:
        case SO_PORT_RELEASE:
        case SO_PORT_LOOKUP: {
            retval = sooptcopyin(sopt, &port_param,
                    sizeof (struct sopt_shared_port_param),
                                    sizeof (struct sopt_shared_port_param));
            if (retval)
                return(retval);
            s = splnet();
            retval = ipv4_control(so, &port_param, kp, sopt->sopt_name);
            splx(s);
            if (retval) {
#if SIP_DEBUG_ERR
                log(LOG_WARNING, "sip_control: ipv4_control returns error=%x for so=%x kp=%x\n",
                        retval, so, kp);
#endif
                return(retval);
            }
            retval = sooptcopyout(sopt, &port_param,
                    sizeof (struct sopt_shared_port_param));
            if (retval)
                return (retval);
            return(EJUSTRETURN);
            break;
        }

        default: {
#if SIP_DEBUG
            log(LOG_WARNING, "sip_control: default unrecognized sopt->sopt_name=%x pass it on\n", sopt->sopt_name);
#endif
            break;
        }
    }
    
    return (0);
}

int
enable_filters(struct socket *so, int reg_flags, void *data, struct kextcb *kp)
{
    register struct blueCtlBlock *ifb;
    struct BlueFilter Filter, *bf;
    int retval, s;

    if ((ifb = (struct blueCtlBlock *)kp->e_fcb) == NULL) {
#if SIP_DEBUG_ERR
        log(LOG_WARNING, "enable_filters: can't find ifb for so=%x kp=%x\n", so, kp);
#endif
        return(ENXIO);
    }
    Filter.BF_flags = reg_flags;
    s = splnet();
    if (reg_flags & SIP_PROTO_ATALK)
        retval = enable_atalk(&Filter, data, ifb);
    else if (reg_flags & SIP_PROTO_IPv4)
        retval = enable_ipv4(&Filter, data, ifb);
    else {
#if SIP_DEBUG_ERR
        log(LOG_WARNING, "enable_filter: unknown protocol requested so=%x kp=%x flags=%x\n",
                so, kp, reg_flags);
#endif
        retval = -EPROTONOSUPPORT;
    }
    /*
        * 'retval' here is either:
        *  >= 0 (an index into the filter array in the ifb struct)
        *  <  0 (negative of a value from errno.h)
        */
    if (retval < 0) {
        splx(s);
        return(-retval);
    }
    bf = &ifb->filter[retval];
    if (Filter.BF_flags & SIP_PROTO_ENABLE) {
        if ((bf->BF_flags & (SIP_PROTO_RCV_FILT)) ==
                SIP_PROTO_RCV_FILT) {
#if SIP_DEBUG_ERR
            log(LOG_WARNING, "enable_filters: SIP_PROTO_ENABLE -already set flags=%x\n",
                    bf->BF_flags);
#endif
            splx(s);
            return(EBUSY);
        }
        *bf = Filter;
    } else if (Filter.BF_flags & SIP_PROTO_DISABLE) {
#if SIP_DEBUG
        log(LOG_WARNING, "enable_filters: SIP_PROTO_DISABLE flags=%x for addr %x.%x\n",
                bf->BF_flags, Filter.BF_address, Filter.BF_node);
#endif
        if (bf->BF_flags & SIP_PROTO_ENABLE)
            bf->BF_flags = 0;
        else {
            splx(s);
            return(EINVAL);
        }
    }
    splx(s);
    return(0);
}

/* Control filter (PF_FILTER) connect */
int
sif_connect(register struct socket *cso)
{
    return(0);
}

void
sif_disconnect()
{
    /* NO-OP */
}

int
sif_read()
{
    return(0);
}

int
sif_write()
{
    return(0);
}

int
sif_get()
{
    return(0);
}

int
sif_put()
{
    return(0);
}

int
si_accept()
{
    return(0);
}

int
si_bind(struct socket *so, struct sockaddr *nam,
	    register struct kextcb *kp)
{
    register struct blueCtlBlock *ifb;

    if ((ifb = _MALLOC(sizeof (struct blueCtlBlock), M_PCB, M_WAITOK)) == NULL) {
#if SIP_DEBUG_ERR
        log(LOG_WARNING, "si_bind: Can't allocate for control block\n");
#endif
        return(ENOBUFS);
    }
    bzero(ifb, sizeof(struct blueCtlBlock));
    TAILQ_INIT(&ifb->fraglist);	/* In case... */
    if (so->so_pcb == NULL) {
#if SIP_DEBUG_ERR
        log(LOG_WARNING, "si_bind: np == NULL\n");
#endif
        if (ifb->dev_media_addr)
            FREE(ifb->dev_media_addr, M_TEMP);
        _FREE(ifb, M_PCB);
        return(EINVAL); /* XXX */
    }
    /*
     * Bump the receive sockbuf size - need a big buffer
     *  to offset the scheduling latencies of the system
     * Try to get something if our grandiose design fails.
     */
    if (sbreserve(&so->so_rcv, 131072) == 0) {
        if (sbreserve(&so->so_rcv, 65536) == 0 &&
            sbreserve(&so->so_rcv, 32768) == 0 &&
            sbreserve(&so->so_rcv, 16384) == 0) {
#if SIP_DEBUG_ERR
            log(LOG_WARNING, "si_bind: so=%x can't  sbreserve enough for the socket..\n", so);
#endif
            return(ENOBUFS);
        }
    }
    kp->e_fcb = (void *)ifb;
    ifb->bcb_link = sipList;
    sipList = ifb;
    ifb->ifb_so = so;
    return(0);
}

/*
 * A client is closing down.  Stop filtering for this socket.
 */
int
si_close(struct socket *so, register struct kextcb *kp)
{
    struct blueCtlBlock *ifb = (struct blueCtlBlock *)kp->e_fcb;
    int retval = 0, s;
    extern void release_ifb(struct blueCtlBlock *);

    s = splnet();
    if ((retval = atalk_stop((struct blueCtlBlock *)kp->e_fcb)) == 0)
    	retval = ipv4_stop((struct blueCtlBlock *)kp->e_fcb);
    if (retval == 0 && ifb != NULL) {
        if (!ifb->fraglist_timer_on)
            release_ifb(ifb);
        else
            ifb->ClosePending = 1;
    }
    splx(s);
    return(retval);
}

int
si_connect()
{
    return(0);
}

int
si_create(struct socket *so, struct protosw *pp,
	register struct kextcb *kp)
{
    kp->e_fcb = (void *)NULL; /* need to stuff something in here? */
#if SIP_DEBUG
    log(LOG_WARNING, "si_create called so=%x with kp=%x\n",so, kp);
#endif
    init_ipv4(so, kp);
    return(0);
}

int
si_discon(struct socket *so, register struct kextcb *kp)
{
#if SIP_DEBUG
    log(LOG_WARNING, "si_discon called so%x with kp=%x\n", so, kp);
#endif
    return(0);
}

int
si_free()
{
    return(0);
}

int
si_sbappend(struct sockbuf *sb, struct mbuf *m, register struct kextcb *kp)
{
#if SIP_DEBUG
    log(LOG_WARNING, "si_sbappend: so=%x called with kp=%x m=%x\n", sb, kp, m);
#endif
    return(0);
}


/* Handle packets coming from Classic */
int
si_send(struct socket *so, struct sockaddr **addr, struct uio **uio,
	struct mbuf **top, struct mbuf **control, int *flags,
	register struct kextcb *kp)
{
    int retval = 0, x;
    struct blueCtlBlock *ifb = (struct blueCtlBlock *)kp->e_fcb;
    struct ifnet *ifp;
    register unsigned char *p;
    register unsigned short *s;

    if (*top == NULL) {
#if SIP_DEBUG
        log(LOG_WARNING, "si_send: ecccck we're hosed...\n");
#endif
        return(0);
    }
    
    /*
     * Check that the ifb is not NULL, we've been getting panics on the
     * first line that dereferences this, so we're adding this check.
     */
    if (ifb == NULL) {
#if SIP_DEBUG_ERROR
        log(LOG_ERROR, "si_send: ifb is NULL!!!\n");
#endif
        return 0;
    }
    
    /*
     * Check that we have a filter installed. If we have no filters
     * installed, we shouldn't be sending packets. This happens in
     * three situations: 1 we haven't setup a filter yet, 2 our
     * filter was destroyed when the interface went away, or 3
     * filtering was turned off.
     */
    if (ifb->atalk_proto_filter_id == 0 &&
        ifb->ipv4_proto_filter_id == 0) {
#if SIP_DEBUG_ERROR
        log(LOG_ERROR, "si_send: no filters enabled!!!\n");
#endif
        m_freem(*top);
        return EJUSTRETURN;
    }
    
    /*
     * Check that the interface is not NULL
     */
    if (so->so_pcb != NULL)
        ifp = ndrv_get_ifp(so->so_pcb);
    else
        ifp = NULL;
    if (ifp == NULL)
    {
        m_freem(*top);
        top = NULL;
        return EJUSTRETURN;
    }
    
    /*
     * Don't need to pull-up since we get "one-buffer" packets from
     *  PF_NDRV output.
     */
    p = mtod(*top, unsigned char *);/* Point to destination media addr */
    x = splnet();
    retval = 0;
    if (ifp->if_type == IFT_ETHER) {
        if ((*top)->m_len >= 22)
        {
            u_int16_t	eType = ntohs(*(u_int16_t*)&p[12]);
            /* These checks are only determing if the packet is 802.3 with a SNAP protocol
             * or Ethernet Type 2. The assumption is that all IP packets will be EType2
             * and all AppleTalk packetes will be 802.3
             */
            if (eType <= ETHERMTU &&
                *(u_int16_t*)&p[14] == 0xaaaa &&
                p[16] == 0x03) /* SNAP */
                retval = si_send_eth_atalk(top, ifb);
            else if (eType == ETHERTYPE_IP ||
                     eType == ETHERTYPE_ARP) /* Is IP or ARP */
                retval = si_send_eth_ipv4(top, ifb, ifp);
            else
                /* Not SNAP, not IP */
                retval = 0;
        }
        else {
#if SIP_DEBUG_ERR
            log(LOG_WARNING, "si_send: mbuf contains less than 22 bytes, can't determine protocol!");
#endif
        }
    } else if (ifp->if_type == IFT_PPP) {
        s = (unsigned short *)p;
        if (*s == 0x21)		/* IPv4 */ {
            struct sockaddr_in sin;
            unsigned long ppp_tag;

            m_adj(*top, sizeof(unsigned short));
            retval = si_send_ppp_ipv4(top, ifb, ifp);
            if (retval) {
                splx(x);
                return(retval);
            }
            /* For PPP, need to pass to higher level support */
            if ((retval = dlil_find_dltag(ifp->if_family,
                                            ifp->if_unit,
                                            PF_INET, &ppp_tag)) != 0) {
                splx(x);
                return(retval);
            }
            bzero((caddr_t)&sin, sizeof(sin));
            sin.sin_family = AF_INET;
            sin.sin_len = sizeof (sin);
            dlil_output(ppp_tag, *top, NULL,
                        (struct sockaddr *)&sin, 0);
            retval = EJUSTRETURN;
            ifb->pkts_out++;
        }
    }
#if SIP_DEBUG_FLOW
    log(LOG_WARNING, "si_send: kp=%x m=%x, disposition %d\n", kp, *top, retval);
#endif
    if (retval == 0)
        ifb->pkts_out++;
    splx(x);
    return(retval);
}

int
si_shutdown(struct sockbuf *so, int how, register struct kextcb *kp)
{
#if SIP_DEBUG
    log(LOG_WARNING, "si_shutdown: so=%x called how=%x for kp=%x\n",so, how, kp);
#endif
    return(0);
}

int
si_soisconnected()
{
    return(0);
}

/* Stop all Sip users, unload if that works */
int
SIP_stop()
{
    struct blueCtlBlock *ifb, *ifb1;
    struct protosw *pp = NULL;
    int retval = KERN_SUCCESS, s;
    int	funnel_state;

    funnel_state = thread_funnel_set(network_flock, TRUE);
#if SIP_DEBUG
    log(LOG_WARNING, "Stopping SIP-NKE\n");
#endif
    s = splnet();
    if ((ifb = sipList) == NULL) {
        splx(s);
        (void) thread_funnel_set(network_flock, funnel_state);
        return(KERN_SUCCESS);
    }
    while (ifb && retval == KERN_SUCCESS) {
        if (atalk_stop(ifb))
            retval = KERN_FAILURE;
        else if (ipv4_stop(ifb))
            retval = KERN_FAILURE;
        else /* else retval was zero == KERN_SUCCESS */ {
            ifb1 = ifb->bcb_link;
            if (ifb->ClosePending || ifb->fraglist_timer_on) {
                retval = KERN_FAILURE;
                ifb->ClosePending = 1;
                break; /* We'll come back later */
            }
            if (ifb->dev_media_addr)
                FREE(ifb->dev_media_addr, M_TEMP);
            _FREE(ifb, M_PCB);
            ifb = ifb1;
        }
    }
    if (retval == KERN_SUCCESS && (pp = pffindproto(PF_NDRV, 0, SOCK_RAW)) == NULL) {
#if SIP_DEBUG_ERR
        log(LOG_WARNING, "Can't find PF_NDRV");
#endif
        retval = KERN_FAILURE;
    } else if ((retval = unregister_sockfilter(&me, pp, 0)) != 0) /* XXX */ {
#if SIP_DEBUG_ERR
        log(LOG_WARNING, "Can't unregister SIP-NKE: retval=%d", retval);
#endif
        retval = KERN_FAILURE;
    }
    splx(s);
    thread_funnel_set(network_flock, funnel_state);
    return(retval);
}

/* blue_inject : append the filtered mbuf to the Blue Box socket and
                 notify BBox that there is something to read.
*/
struct sockaddr_dl ndrvsrc = {sizeof (struct sockaddr_dl), AF_NDRV};

int
blue_inject(struct blueCtlBlock *ifb, register struct mbuf *m)
{
    int s;

    /* move packet from if queue to socket */
    /* !!!Fix this to work generically!!! */
    ndrvsrc.sdl_type = IFT_ETHER;
    ndrvsrc.sdl_nlen = 0;
    ndrvsrc.sdl_alen = 6;
    ndrvsrc.sdl_slen = 0;
    bcopy(m->m_data+6, &ndrvsrc.sdl_data, ndrvsrc.sdl_alen);

#if SIP_DEBUG_FLOW
    log(LOG_WARNING, "blue_inject: ifb=%x so=%x so_rcv=%x m=%x\n", ifb,
        ifb->ifb_so, &ifb->ifb_so->so_rcv, m);
#endif
    if (!ifb || !ifb->ifb_so) {
#if SIP_DEBUG
        log(LOG_WARNING, "blue_inject: no valid ifb for m=%x\n", m);
#endif
        m_freem(m);
        return (-1); /* argh! */
    }
    s = splnet();
    if (sbappendaddr(&(ifb->ifb_so->so_rcv),
                        (struct sockaddr *)&ndrvsrc, m,
                        (struct mbuf *)0) == 0) {
        /* yes, sbappendaddr returns zero if the sockbuff is full... */
        ifb->full_sockbuf++;
#if SIP_DEBUG
        log(LOG_WARNING, "blue_inject: sbappendaddr socket full for so=%x\n", ifb->ifb_so);
#endif
        if (m)
            m_free(m);
        splx(s);
        return(ENOMEM);
    } else {
#if SIP_DEBUG_FLOW
        log(LOG_WARNING, "blue_inject: call sorwakeup for so=%x\n",
                ifb->ifb_so);
#endif
        sorwakeup(ifb->ifb_so);     /* Start by using SIGIO */
        ifb->pkts_up++;
        splx(s);
        return(0);
    }
}

int
my_frameout(struct mbuf **m0,
            struct ifnet *ifp,
            char *dest_linkaddr,
            char *ether_type)
{
    register struct mbuf *m = *m0;
    register struct ether_header *eh;
    int hlen;       /* link layer header lenght */
    struct arpcom *ac = (struct arpcom *)ifp;

    if ((m->m_flags & M_PKTHDR) == 0) {
#if SIP_DEBUG
        log(LOG_WARNING, "my_frameout: m=%x m_flags=%x doesn't have PKTHDR set\n", m, m->m_flags);
#endif
        m_freem(m);
        return (1);
    }
    hlen = ETHER_HDR_LEN;
    /*
     * Add local net header.  If no space in first mbuf,
     * allocate another.
     */
    M_PREPEND(m, sizeof (struct ether_header), M_DONTWAIT);
    if (m == 0) {
#if SIP_DEBUG
         log(LOG_WARNING, "my_frameout: can't prepend\n");
#endif
         return (1);
    }
    *m0 = m;
    eh = mtod(m, struct ether_header *);
    if (ether_type != NULL)
    {
        memcpy(&eh->ether_type, ether_type,
                sizeof(eh->ether_type));
    }
    else
    {
        // 802.3, eh->ether_type should be set to
        // the length of the packet
        u_int16_t	length = 0;
        struct mbuf* cur = *m0;
        while (cur != NULL)
        {
            length += cur->m_hdr.mh_len;
            cur = cur->m_hdr.mh_next;
        }
        length -= sizeof(struct ether_header);
        
        eh->ether_type = htons(length);
    }
    memcpy(eh->ether_dhost, dest_linkaddr, 6);
    memcpy(eh->ether_shost, ac->ac_enaddr,
        sizeof(eh->ether_shost));
    m->m_flags |= 0x10;
    *m0 = m;
    return (0);
}

void
release_ifb(struct blueCtlBlock *ifb)
{
    struct blueCtlBlock *current, *prev;

    if (ifb == NULL)
        return;
    if (ifb->dev_media_addr)
        FREE(ifb->dev_media_addr, M_TEMP);
    for (current = sipList, prev = NULL; current;)
        if (current == ifb) {
            if (prev)
                prev->bcb_link = ifb->bcb_link;
            else
                sipList = ifb->bcb_link;
            break;
        } else {
            prev = current;
            current = current->bcb_link;
        }
    _FREE(ifb, M_PCB);
    ifb = NULL;
}


int do_pullup(struct mbuf **m_orig, unsigned int inSizeNeeded)
{
    struct mbuf* newM = *m_orig;
    
    if (newM->m_pkthdr.len < inSizeNeeded) {
#if SIP_DEBUG
        log(LOG_WARNING, "do_pullup %8.8X, wanted %d bytes, only %d in packet!",
                newM, inSizeNeeded, newM->m_pkthdr.len);
#endif
        return ENOENT;
    }
    while ((newM->m_len < inSizeNeeded) && newM->m_next) {
        unsigned int total = 0;
        total = newM->m_len + (newM->m_next)->m_len;
        
        if ((newM = m_pullup(newM, min(inSizeNeeded, total))) == NULL) {
#if SIP_DEBUG
            log(LOG_WARNING, "do_pullup(%8.8X, %d) - out of memory!", *m_orig, inSizeNeeded);
#endif
            /* Packet has been destroyed. */
            *m_orig = newM;
            return ENOMEM;
        } else
            *m_orig = newM;
    }
    return 0;
}
