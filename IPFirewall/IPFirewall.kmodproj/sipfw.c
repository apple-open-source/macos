/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All rights reserved.
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

#define _IP_VHL

#include <mach/mach_types.h>

#include <machine/spl.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/queue.h>

#include <net/kext_net.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/ethernet.h>
#include <net/ndrv.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <kern/queue.h> 

#include "ip_fw.h"
#include "sipfw.h"

#define MAX_FRAME_SAVED 16

extern struct NFDescriptor sipfwldr_nfd;
extern u_int16_t ip_divert_cookie;
extern struct sockaddr_in *ip_fw_fwd_addr;

extern int ip_fw_chk(struct ip **pip, int hlen,
                     struct ifnet *oif, u_int16_t *cookie, struct mbuf **m,
                     struct ip_fw_chain **flow_id,
                     struct sockaddr_in **next_hop);


static int sipfw_soclose(struct socket *, struct kextcb *);
static int sipfw_socreate(struct socket *, struct protosw *, struct kextcb *);
static int sipfw_sofree(struct socket *, struct kextcb *);
static int sipfw_sosend(struct socket *, struct sockaddr **, struct uio **,
                 struct mbuf **, struct mbuf **, int *,
                 struct kextcb *);

static struct sockif sipfw_sockif = {
    NULL,		/* soabort */
    NULL,			/* soaccept */
    NULL,	/* sobind */
    sipfw_soclose,	/* soclose */
    NULL,			/* soconnect */
    NULL,		/* soconnect2 */
    NULL,	/* soset/getopt */
    sipfw_socreate,	/* socreate */
    NULL,	/* sodisconnect */
    sipfw_sofree,			/* sofree */
    NULL,		/* sogetopt */
    NULL,		/* sohasoutofband */
    NULL,		/* solisten */
    NULL,		/* soreceive */
    NULL,		/* sorflush */
    sipfw_sosend,	/* sosend */
    NULL,		/* sosetopt */
    NULL,	/* soshutdown */
    NULL,		/* socantrcvmore */
    NULL,		/* socantsendmore */
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

static int sipfw_sbappendaddr(struct sockbuf *, struct sockaddr *,
                       struct mbuf *, struct mbuf *, struct kextcb *);

static struct sockutil sipfw_sockutil = {
    NULL, /* sb_lock */
    NULL, /* sbappend */
    sipfw_sbappendaddr, /* sbappendaddr */
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


static struct NFDescriptor sipfw_nfd =
{
    {NULL, NULL},
    {NULL, NULL},
    SIPFW_NFHANDLE,
    NFD_PROG|NFD_VISIBLE,	/* only if we want global filtering */
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &sipfw_sockif,
    &sipfw_sockutil
};

static int sipfw_inited = 0;
static int sipfw_enable_in = 1;
static int sipfw_enable_out = 1;
static int sipfw_debug = 0;
static int sipfw_usecount = 0;

SYSCTL_DECL(_net_inet_ip_fw);
SYSCTL_NODE(_net_inet_ip_fw, OID_AUTO, classic, CTLFLAG_RW, 0, "Classic");
SYSCTL_INT(_net_inet_ip_fw_classic, OID_AUTO, enable_in, CTLFLAG_RW, &sipfw_enable_in, 0, "");
SYSCTL_INT(_net_inet_ip_fw_classic, OID_AUTO, enable_out, CTLFLAG_RW, &sipfw_enable_out, 0, "");
SYSCTL_INT(_net_inet_ip_fw_classic, OID_AUTO, debug, CTLFLAG_RW, &sipfw_debug, 0, "");
SYSCTL_INT(_net_inet_ip_fw_classic, OID_AUTO, usecount, CTLFLAG_RW, &sipfw_usecount, 0, "");

extern kern_return_t sipfwldr_load();
extern kern_return_t sipfwldr_unload();

#if DEBUG
#define DBG_PRINTF(x) if (sipfw_debug) { printf x ; }
#define LOG_PRINTF(x) printf x

static void DumpHex(const char *msg, void *where, size_t len)
{
    size_t		i;
    unsigned char	*p = (unsigned char *)where;

    
    if (msg)
        DBG_PRINTF(("%s\n", msg));
    
    for (i = 0; i < len; i += 16) {
        int	j;

        DBG_PRINTF(("%4lu: ", i));

        for (j = i; j < len && j < i + 16; j++)
            DBG_PRINTF(("%02x ", p[j]));

        for (;j < i + 16; j++)
            DBG_PRINTF(("   "));

        for (j = i; j < len && j < i + 16; j++)
            DBG_PRINTF(("%c", p[j] >= 0x20 && p[j] < 0x7f ? p[j] : '.'));

        DBG_PRINTF(("\n"));
    }
}
#else
#define DBG_PRINTF(x) 
#define LOG_PRINTF(x) 
#define DumpHex(msg, where, len)
#endif /* 0 */


int sipfw_soclose(struct socket *so, struct kextcb *kp)
{
    DBG_PRINTF(("sipfw_soclose: so=%x kp=%x\n", so, kp));
    sipfw_usecount--;
    return 0;
}

int sipfw_socreate(struct socket *so, struct protosw *proto, struct kextcb *kp)
{
    DBG_PRINTF(("sipfw_socreate: so=%x protp=%x kp=%x\n", so, proto, kp));
    sipfw_usecount++;
    return 0;
}

int sipfw_sofree(struct socket *so, struct kextcb *kp)
{
    DBG_PRINTF(("sipfw_sofree: so=%x kp=%x\n", so, kp));
    return 0;
}

/*
 * Returns 0 in case of success
 * The packet may be reallocated and moified provided top gets updated
 */
int sipfw_sosend(struct socket *so, struct sockaddr **sa, struct uio **uio,
     struct mbuf **top, struct mbuf **control, int *flags,
     struct kextcb *kp)
{
    struct ifnet	*ifp;
    int				is_our = 0;
    struct mbuf		*m0;
    int				error = 0;
    int				frame_len = 0;
    char			frame_header[sizeof(struct ether_header)];

    if (!sipfw_enable_out)
        return 0;
    
    if ((m0 = *top) == NULL) {
        LOG_PRINTF(("sipfw_sosend: top is null\n"));
        return 0;
    }
    
    if (m0->m_nextpkt)
        panic("sipfw_sbappendaddr m_orig->m_nextpkt");

    ifp = (struct ifnet *)((struct ndrv_cb *)so->so_pcb)->nd_if;
    if (!ifp) {
        LOG_PRINTF(("sipfw_sosend nd_if is NULL\n"));
        return 0;
    }
    
    DBG_PRINTF(("sipfw_sosend: m_len=%x m_flags=%x m_pkthdr.len=%x \n",
            m0->m_len, m0->m_flags, m0->m_pkthdr.len));

    switch (ifp->if_type) {
        case IFT_ETHER: {
            struct ether_header	*eh;

            /*
             * Check that we have at least an Ethernet packet with IP header 
             */
            if (m0->m_pkthdr.len < sizeof(struct ether_header) + sizeof(struct ip))
                return 0;
            /*
             * To simplify restoration of complete frame let's make room for both
             * the frame header and the IP header
             */
            if (m0->m_len < sizeof(struct ether_header) + sizeof(struct ip)) {
                if ((m0 = m_pullup(m0, sizeof(struct ether_header) + sizeof(struct ip))) == NULL) {
                    /* The original mbuf got destroyed */
                    return EJUSTRETURN;
                }
            }
            eh = mtod(m0, struct  ether_header *);
            if (ntohs(eh->ether_type) == ETHERTYPE_IP) {
                frame_len = sizeof(struct  ether_header);
                bcopy(eh, frame_header, frame_len);
                m_adj(m0, frame_len);
                is_our = 1;
            }
            break;
        }
        case IFT_PPP: {
            unsigned short *s;

            /*
             * Check that we have at least an Ethernet packet with IP header 
             */
            if (m0->m_pkthdr.len < sizeof(unsigned short) + sizeof(struct ip))
                return 0;

            s = mtod(*top, unsigned short *); /* Point to destination media addr */
            if (*s == 0x0021) {	/* IPv4 communication header Classic/SIP*/
                frame_len = sizeof(unsigned short);
                bcopy(s, frame_header, frame_len);
                m_adj(*top, sizeof(unsigned short));
                is_our = 1;
            }
            break;
        }
        default:
            break;
    }
    if (is_our) {
        struct ip			*ip;
        int					hlen;
        struct sockaddr_in 	sin, *dst = &sin;

        ip = mtod(m0, struct ip *);

        hlen = IP_VHL_HL(ip->ip_vhl) << 2;

        dst->sin_family = AF_INET;
        dst->sin_len = sizeof(*dst);
        dst->sin_addr = ip->ip_dst;

        if (ip_fw_chk_ptr) {
            struct ip_fw_chain	*rule = NULL ;
            int					off;
            struct sockaddr_in *old = dst;

            off = (*ip_fw_chk_ptr)(&ip,
                                   hlen, ifp, &ip_divert_cookie, &m0, &rule, &dst);
            /*
             * On return we must do the following:
             * m == NULL         -> drop the pkt
             * 1<=off<= 0xffff   -> DIVERT
             * (off & 0x10000)   -> send to a DUMMYNET pipe
             * dst != old        -> IPFIREWALL_FORWARD
             * off==0, dst==old  -> accept
             * If some of the above modules is not compiled in, then
             * we should't have to check the corresponding condition
             * (because the ipfw control socket should not accept
                * unsupported rules), but better play safe and drop
             * packets in case of doubt.
             */
            if (!m0) {
                /* firewall said to reject */
                DBG_PRINTF(("sipfw_sosend discarded by firewall\n"));
                error = EJUSTRETURN;
                goto done;
            }
            if (off == 0 && dst == old) /* common case */
                goto pass ;
#if DUMMYNET
            if (off & 0x10000) {
                /*
                 * pass the pkt to dummynet. Need to include
                 * pipe number, m, ifp, ro, hlen because these are
                 * not recomputed in the next pass.
                 * All other parameters have been already used and
                 * so they are not needed anymore.
                 * XXX note: if the ifp or ro entry are deleted
                 * while a pkt is in dummynet, we are in trouble!
                 *
                 * %%% TBD Need to compute ro
                 */
                DBG_PRINTF(("sipfw_sosend dummynet\n"));
                dummynet_io(off & 0xffff, DN_TO_IP_OUT, m,ifp,ro,hlen,rule);
                error = EJUSTRETURN;
                goto done;
            }
#endif
#if IPDIVERT
            if (off > 0 && off < 0x10000) {         /* Divert packet */
                DBG_PRINTF(("sipfw_sosend divert\n"));
                ip_divert_port = off & 0xffff ;
                (*ip_protox[IPPROTO_DIVERT]->pr_input)(m0, 0);
                error = EJUSTRETURN;
                goto done;
            }
#endif
            /*
             * if we get here, none of the above matches, and
             * we have to drop the pkt
             */
            DBG_PRINTF(("sipfw_sosend drop\n"));
            m_freem(m0);
            m0 = 0;
            frame_len = 0;
            error = EJUSTRETURN;
            goto done;
pass:
            DBG_PRINTF(("sipfw_sosend pass\n"));
        }
    }
done:
    /*
        * Don't forget to restore the frame when passing the packet
        */
    if (m0 && frame_len > 0) {
        if (m_leadingspace(m0) < frame_len) {
            m_freem(m0);
            m0 = 0;
            printf("sipfw_sosend no lead space");
        } else {
            m0->m_len += frame_len;
            m0->m_data -= frame_len;
            m0->m_pkthdr.len += frame_len;
        }
    }
    *top = m0;
    return error;
}

/*
 * Make a carbon copy of a packet header and replace it
 */
static void m_replace_hdr(struct mbuf *m_from, struct mbuf *m_to)
{
    if (!(m_from->m_flags & M_PKTHDR))
        panic("m_replace_hdr m_from not M_PKTHDR");

    /* Copy the whole stuff */
    bcopy(m_from, m_to, sizeof(struct mbuf));

    /* Update the external reference otherwise we'll leak the cluster */
    if (m_from->m_flags & M_EXT) {
        m_to->m_ext.ext_refs.forward = m_to->m_ext.ext_refs.backward = \
                        &m_to->m_ext.ext_refs;
        if (MCLHASREFERENCE(m_from)) {
            insque((queue_t)&m_to->m_ext.ext_refs, (queue_t)&m_from->m_ext.ext_refs);
            remque((queue_t)&m_from->m_ext.ext_refs);
        }
    }
    
    /* Reset the original packet header */
    m_from->m_next = m_from->m_nextpkt = NULL;
    m_from->m_len = 0;
    m_from->m_data = m_from->m_pktdat;
    m_from->m_flags = M_PKTHDR;
    m_from->m_pkthdr.rcvif = m_from->m_pkthdr.header = NULL;
    m_from->m_pkthdr.csum_flags = m_from->m_pkthdr.csum_data = 0;
    m_from->m_pkthdr.aux = (struct mbuf *)NULL; 
    m_from->m_pkthdr.reserved1 = m_from->m_pkthdr.reserved2 = NULL;  
}

/*
 * Note:
 * Like all socket filter routines, a sbappendaddr filter returns 0 upon success,
 * this is unlike the BSD sbappendaddr() that returns 1 upon success and 0 to
 * indicate a failure.
 * In case of error, that's the caller responsibility to free the packet
 * One cano
 */
int sipfw_sbappendaddr(struct sockbuf *sb, struct sockaddr *asa,
                       struct mbuf *m_orig, struct mbuf *control, struct kextcb *kp)
{
    struct ifnet *		rif;
    int				frame_len = 0;
    char			frame_header[sizeof(struct ether_header)];
    int				is_our = 0;
    struct ip			*ip = NULL;
    int				i, hlen;
    u_short			sum;
    struct ip_fw_chain		*rule = NULL;
    struct mbuf 		*m0 = NULL;

    if (!sipfw_enable_in)
        return 0;
    
    /* It's conceivable that no data is added to the socket buffer */
    if (!m_orig) {
        return 0;
    }
    if (m_orig->m_nextpkt)
        panic("sipfw_sbappendaddr m_orig->m_nextpkt");
    
    rif = m_orig->m_pkthdr.rcvif;
    if (rif == NULL) {
        rif = (struct ifnet *)((struct ndrv_cb *)sbtoso(sb)->so_pcb)->nd_if;
        if (rif == NULL) {
            LOG_PRINTF(("sipfw_sbappendaddr nd_if is NULL\n"));
            return 1;	/* Fail if cannot filter */
        }
    }
    DBG_PRINTF(("sipfw_sbappendaddr: sb=%x asa=%x m_orig=%x control=%x kp=%x\n",
                sb, asa, m_orig, control, kp));
    
    /* 
     * Copy the mbuf so that the mbuf is the same when we return even if 
     * we drop it or we do a pullup
     */
    MGETHDR(m0, M_DONTWAIT, m_orig->m_type);
    if (m0 == 0) 
        return 1;	/* Fail if cannot filter */
    m_replace_hdr(m_orig, m0);
    
    switch (rif->if_type) {
        case IFT_ETHER: {
            /*
             * Check that we have at least an Ethernet packet with IP header 
             */
            if (m0->m_pkthdr.len >= sizeof(struct ether_header) + sizeof(struct ip)) {
                struct ether_header	*eh;
    
                /*
                * To simplify restoration of complete frame let's make room for both
                * the frame header and the IP header
                */
                if (m0->m_len < sizeof(struct ether_header)) {
                    m0 = m_pullup(m0, sizeof (struct ether_header));
                    if (!m0)
                        goto drop;
                }
                eh = mtod(m0, struct  ether_header *);
                if (ntohs(eh->ether_type) == ETHERTYPE_IP) {
                    frame_len = sizeof(struct  ether_header);
                    bcopy(eh, frame_header, frame_len);
                    m_adj(m0, frame_len);
                    is_our = 1;
                }
            }
            break;
        }
        case IFT_PPP:
            is_our = 1;
            break;
        case IFT_LOOP:
            is_our = 1;
            break;
        default:
            break;
    }
    if (is_our) {
        DBG_PRINTF(("sipfw_sbappendaddr is our\n"));
        if (m0->m_pkthdr.len < sizeof(struct ip)) {
            DBG_PRINTF(("sipfw_sbappendaddr packet smaller than IP header\n"));
            goto drop;
        }

        if (m0->m_len < sizeof(struct ip) &&
            (m0 = m_pullup(m0, sizeof (struct ip))) == 0) {
            DBG_PRINTF(("sipfw_sbappendaddr m_pullup (struct ip) failed\n"));
            goto drop;
        }
        ip = mtod(m0, struct ip *);

        if (IP_VHL_V(ip->ip_vhl) != IPVERSION) {
            DumpHex("sipfw_sbappendaddr ip", ip, MIN(ntohs(ip->ip_len), 64));
            DBG_PRINTF(("sipfw_sbappendaddr not a V4 IP packet\n"));
            goto drop;
        }

        hlen = IP_VHL_HL(ip->ip_vhl) << 2;
        if (hlen < sizeof(struct ip)) { /* minimum header length */
            DBG_PRINTF(("sipfw_sbappendaddr IP header length too small\n"));
            goto drop;
        }
        if (hlen > m0->m_len) {
            if ((m0 = m_pullup(m0, hlen)) == 0) {
                DBG_PRINTF(("sipfw_sbappendaddr m_pullup (hlen) failed\n"));
                goto drop;
            }
            ip = mtod(m0, struct ip *);
        }

        sum = in_cksum(m0, hlen);

        if (sum) {
            DBG_PRINTF(("sipfw_sbappendaddr bad checksum\n"));
            goto drop;
        }
        /*
         * Convert fields to host representation.
         */
        NTOHS(ip->ip_len);
        if (ip->ip_len < hlen) {
            DBG_PRINTF(("sipfw_sbappendaddr ip_len < hlen\n"));
            goto drop;
        }
        NTOHS(ip->ip_id);
        NTOHS(ip->ip_off);

        /*
         * Check that the amount of data in the buffers
         * is as at least much as the IP header would have us expect.
         * Trim mbufs if longer than we expect.
         * Drop packet if shorter than we expect.
         */
        if (m0->m_pkthdr.len < ip->ip_len) {
            DBG_PRINTF(("sipfw_sbappendaddr m_pkthdr.len < ip_len < hlen\n"));
            goto drop;
        }
        if (m0->m_pkthdr.len > ip->ip_len) {
            if (m0->m_len == m0->m_pkthdr.len) {
                m0->m_len = ip->ip_len;
                m0->m_pkthdr.len = ip->ip_len;
            } else
                m_adj(m0, ip->ip_len - m0->m_pkthdr.len);
        }
        if (ip_fw_chk_ptr) {
#if IPFIREWALL_FORWARD
            /*
             * If we've been forwarded from the output side, then
             * skip the firewall a second time
             */
            if (ip_fw_fwd_addr) {
                DBG_PRINTF(("sipfw_sbappendaddr forwarded from output side\n"));
                goto pass;
            }
#endif  /* IPFIREWALL_FORWARD */
            /*
             * HACK ATTACK:
             * Pass 1 has oif so that the mbuf does not get free
             */
            i = (*ip_fw_chk_ptr)(&ip, hlen, NULL, &ip_divert_cookie,
                                 &m0, &rule, &ip_fw_fwd_addr);
            /*
             * see the comment in ip_output for the return values
             * produced by the firewall.
             */
            if (!m0) {
                /* packet discarded by firewall */
                DBG_PRINTF(("sipfw_sbappendaddr discarded by firewall\n"));
                goto drop;
            }
            if (i == 0 && ip_fw_fwd_addr == NULL) /* common case */
                goto pass ;
#if DUMMYNET
            if (i & 0x10000) {
                /* send packet to the appropriate pipe: NOT! */
                DBG_PRINTF(("sipfw_sbappendaddr drop dummy net\n"));
                goto drop;
            }
#endif
#if IPDIVERT
            if (i > 0 && i < 0x10000) {
                /* Divert packet */
                DBG_PRINTF(("sipfw_sbappendaddr drop divert\n"));
                goto drop;
            }
#endif
#if IPFIREWALL_FORWARD
            if (i == 0 && ip_fw_fwd_addr != NULL)
                goto pass ;
#endif
            /*
             * if we get here, the packet must be dropped
             *
             * Pretend all went OK so that caller won't call m_free again !
             */
            DBG_PRINTF(("sipfw_sbappendaddr drop packet\n"));
            goto drop;
        }
    } else {
        goto pass;
    }

drop:
    DBG_PRINTF(("sipfw_sbappendaddr: drop m_orig %p m0 %p", m_orig, m0));
#if IPFIREWALL_FORWARD
        /*
            * We do not forward packets when they are on their way to Classic
            */
        ip_fw_fwd_addr = NULL;
#endif
    /*
        * It's the caller's reponsibility to free the mbuf
        */
    if (m0)
        m_freem(m0);
    return 1;
    
pass:
    DBG_PRINTF(("sipfw_sbappendaddr: pass m_orig %p m0 %p", m_orig, m0));
#if IPFIREWALL_FORWARD
    /*
        * We do not forward packets on their way to Classic
        */
    ip_fw_fwd_addr = NULL;
#endif
    /*
        * The packet may have been modified with pullup
        * Don't forget to restore the frame when passing the packet
        */
    if (frame_len > 0) {
        if (frame_len > m_leadingspace(m0)) {
            m0 = m_prepend(m0, frame_len, M_DONTWAIT);
            if (m0 == 0)
                goto drop;
        }
        m0->m_len += frame_len;
        m0->m_data -= frame_len;
        m0->m_pkthdr.len += frame_len;
        bcopy(frame_header, mtod(m0, char *), frame_len);
    }
    
    m_replace_hdr(m0, m_orig);
    m_freem(m0);
    return 0;
}


kern_return_t sipfw_load()
{
    kern_return_t	err = KERN_FAILURE;
    struct protosw *pp;
    spl_t spl;

    DBG_PRINTF(("sipfw_load\n"));

    spl = splnet();

    if ((pp = pffindproto(PF_NDRV, 0, SOCK_RAW)) == NULL)
    {
        DBG_PRINTF(("sipfw_load: Can't find PF_NDRV"));
        goto done;
    }
    if (register_sockfilter(&sipfw_nfd, NULL, pp, 0))
    {
        DBG_PRINTF(("sipfw_load: Can't register sipfw_nfd"));
        goto done;
    }

    sysctl_register_oid(&sysctl__net_inet_ip_fw_classic);
    sysctl_register_oid(&sysctl__net_inet_ip_fw_classic_enable_in);
    sysctl_register_oid(&sysctl__net_inet_ip_fw_classic_enable_out);
    sysctl_register_oid(&sysctl__net_inet_ip_fw_classic_debug);
    sysctl_register_oid(&sysctl__net_inet_ip_fw_classic_usecount);

    if ((err = sipfwldr_load()) != KERN_SUCCESS)
    {
        DBG_PRINTF(("sipfw_load: sipfwldr_load failed"));
        goto done;
    }

    sipfw_inited = 1;
    err = KERN_SUCCESS;
done:
    splx(spl);

    return err;
}

kern_return_t sipfw_unload()
{
    kern_return_t	err = KERN_FAILURE;
    struct protosw *pp;
    spl_t spl;

    DBG_PRINTF(("sipfw_unload\n"));

    spl = splnet();

    if ((err = sipfwldr_unload()) != KERN_SUCCESS)
    {
        DBG_PRINTF(("sipfw_unload: sipfwldr_unload failed"));
        goto done;
    }
    if ((pp = pffindproto(PF_NDRV, 0, SOCK_RAW)) == NULL)
    {
        DBG_PRINTF(("sipfw_unload: Can't find PF_NDRV"));
        goto done;
    }
    if (unregister_sockfilter(&sipfw_nfd, pp, 0))
    {
        DBG_PRINTF(("sipfw_unload: Can't unregister sipfw_nfd"));
        goto done;
    }
    sysctl_unregister_oid(&sysctl__net_inet_ip_fw_classic_usecount);
    sysctl_unregister_oid(&sysctl__net_inet_ip_fw_classic_enable_in);
    sysctl_unregister_oid(&sysctl__net_inet_ip_fw_classic_enable_out);
    sysctl_unregister_oid(&sysctl__net_inet_ip_fw_classic_debug);
    sysctl_unregister_oid(&sysctl__net_inet_ip_fw_classic);

    sipfw_inited = 0;
    err = KERN_SUCCESS;
done:
    splx(spl);

    return err;
}

