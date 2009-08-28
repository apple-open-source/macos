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
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/in_systm.h>
#include <net/bpf.h>
#include <net/kpi_interface.h>
#include <net/if.h>

#include "slcompress.h"
#include "ppp_defs.h"		// public ppp values
#include "if_ppp.h"		// public ppp API
#include "if_ppplink.h"		// public link API
#include "ppp_domain.h"
#include "ppp_if.h"
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

static errno_t ppp_if_output(ifnet_t ifp, mbuf_t m);
static void ppp_if_if_free(ifnet_t ifp);
static errno_t ppp_if_ioctl(ifnet_t ifp, u_long cmd, void *data);
static int  ppp_if_demux(ifnet_t ifp, mbuf_t m, char *frame_header,
                  protocol_family_t *protocol_family);
static int  ppp_if_add_proto(ifnet_t ifp, protocol_family_t protocol_family,
			const struct ifnet_demux_desc *demux_list, u_int32_t demux_count);
static int  ppp_if_del_proto(ifnet_t ifp, protocol_family_t protocol_family);
static errno_t  ppp_if_frameout(ifnet_t ifp, mbuf_t *m0,
                     const struct sockaddr *ndest, const char *edst, const char *ppp_type);

static int 	ppp_if_detach(ifnet_t ifp);
static struct ppp_if *ppp_if_findunit(u_short unit);
static int ppp_if_set_bpf_tap(ifnet_t ifp, bpf_tap_mode mode, bpf_packet_func func);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

static TAILQ_HEAD(, ppp_if) 	ppp_if_head;
static lck_grp_attr_t			*ppp_if_lck_grp_attr = 0;
static lck_attr_t				*ppp_if_lck_attr = 0;
static lck_grp_t				*ppp_if_lck_grp = 0;

extern lck_mtx_t				*ppp_domain_mutex;

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_if_init()
{
    TAILQ_INIT(&ppp_if_head);

	ppp_if_lck_grp_attr = lck_grp_attr_alloc_init();
	LOGNULLFAIL(ppp_if_lck_grp_attr, "ppp_if_init: lck_grp_attr_alloc_init failed\n");

	lck_grp_attr_setdefault(ppp_if_lck_grp_attr);
				
	ppp_if_lck_grp = lck_grp_alloc_init("PPP", ppp_if_lck_grp_attr);
	LOGNULLFAIL(ppp_if_lck_grp, "ppp_if_init: lck_grp_alloc_init failed\n");
	
	ppp_if_lck_attr = lck_attr_alloc_init();
	LOGNULLFAIL(ppp_if_lck_attr, "ppp_if_init: lck_attr_alloc_init failed\n");

	lck_attr_setdefault(ppp_if_lck_attr);
	//lck_attr_setdebug(ppp_if_lck_attr);
	
    return 0;
	
fail:
	if (ppp_if_lck_grp) {
		lck_grp_free(ppp_if_lck_grp);
		ppp_if_lck_grp = 0;
	}
	if (ppp_if_lck_grp_attr) {
		lck_grp_attr_free(ppp_if_lck_grp_attr);
		ppp_if_lck_grp_attr = 0;
	}
	if (ppp_if_lck_attr) {
		lck_attr_free(ppp_if_lck_attr);
		ppp_if_lck_attr = 0;
	}
	return KERN_FAILURE;

}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_if_dispose()
{

    // can't dispose if interface are in use
    if (!TAILQ_EMPTY(&ppp_if_head))
        return EBUSY;

	lck_grp_free(ppp_if_lck_grp);
	ppp_if_lck_grp = 0;

	lck_grp_attr_free(ppp_if_lck_grp_attr);
	ppp_if_lck_grp_attr = 0;

	lck_attr_free(ppp_if_lck_attr);
	ppp_if_lck_attr = 0;
        
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_if_attach(u_short *unit)
{
    int 		ret = 0;	
    struct ppp_if  	*wan, *wan1;
	struct ifnet_init_params init;
	struct ifnet_stats_param stats;
	
    MALLOC(wan, struct ppp_if *, sizeof(struct ppp_if), M_TEMP, M_WAITOK);
    if (!wan)
        return ENOMEM;
		
    bzero(wan, sizeof(struct ppp_if));
	wan->unit = 0xFFFF;
	
	wan1 = TAILQ_FIRST(&ppp_if_head);

    if (*unit != 0xFFFF) {
		// if a specific nuber has been requested, find if not in use, and insert it
		while (wan1) {
			if (wan1->unit == *unit) {
				// in use, just return error.
				lck_mtx_unlock(ppp_domain_mutex);
				ret = EINVAL;
				goto error_nolock;
			}
			if (wan1->unit > *unit)
				break;				
			wan1 = TAILQ_NEXT(wan1, next);
		}
    }
	else {
		// find a free unit and insert it, keep the list ordered
		*unit = 0;
		wan1 = TAILQ_FIRST(&ppp_if_head);
		while (wan1) {
			if (wan1->unit > *unit)
				break;
			*unit = wan1->unit + 1;
			wan1 = TAILQ_NEXT(wan1, next);
		}
	}
	
	wan->mtx = lck_mtx_alloc_init(ppp_if_lck_grp, ppp_if_lck_attr);
	if (wan->mtx == 0) {
		lck_mtx_unlock(ppp_domain_mutex);
		ret = ENOMEM;
		goto error_nolock;
	}

	wan->unit = *unit;
	if (wan1)
		TAILQ_INSERT_BEFORE(wan1, wan, next);
	else
		TAILQ_INSERT_TAIL(&ppp_if_head, wan, next);

    bzero(&init, sizeof(init));
	init.name = APPLE_PPP_NAME;
	init.unit = *unit;
	init.family = IFNET_FAMILY_PPP;
	init.type = IFT_PPP;
	init.output = ppp_if_output;
	init.demux = ppp_if_demux;
	init.add_proto = ppp_if_add_proto;
	init.del_proto = ppp_if_del_proto;
	init.framer = ppp_if_frameout;
	init.softc = wan;
	init.ioctl = ppp_if_ioctl;
	init.detach = ppp_if_if_free;
	init.set_bpf_tap = ppp_if_set_bpf_tap;
	
	lck_mtx_unlock(ppp_domain_mutex);

	ret = ifnet_allocate(&init, &wan->net);
    if (ret)
        goto error_nolock;

    TAILQ_INIT(&wan->link_head);

	ifnet_set_hdrlen(wan->net, PPP_HDRLEN);
	ifnet_set_flags(wan->net, IFF_POINTOPOINT | IFF_MULTICAST, 0xFFFF); // || IFF_RUNNING
	ifnet_set_mtu(wan->net, PPP_MTU);
	ifnet_set_baudrate(wan->net, 0 /* 10 Mbits/s ??? */);
	bzero(&stats, sizeof(stats));
	ifnet_set_stat(wan->net, &stats);
	ifnet_touch_lastchange(wan->net);
	
    ret = ifnet_attach(wan->net, NULL);
    if (ret)
        goto error_nolock;
    
    bpfattach(wan->net, DLT_PPP, PPP_HDRLEN);

    // attach network protocols

    wan->sndq.maxlen = IFQ_MAXLEN;
    wan->npmode[NP_IP] = NPMODE_ERROR;
    wan->npmode[NP_IPV6] = NPMODE_ERROR;

	lck_mtx_lock(ppp_domain_mutex);
    return 0;

error_nolock:
    if (wan->net)
        ifnet_release(wan->net);
	if (wan->mtx)
		lck_mtx_free(wan->mtx, ppp_if_lck_grp);
	lck_mtx_lock(ppp_domain_mutex);
	if (wan->unit != 0xFFFF) {
		TAILQ_REMOVE(&ppp_if_head, wan, next);
	}
	FREE(wan, M_TEMP);
    return ret;
}

/* -----------------------------------------------------------------------------
detach ppp interface from dlil layer
----------------------------------------------------------------------------- */
int ppp_if_detach(ifnet_t ifp)
{
    struct ppp_if  	*wan = ifnet_softc(ifp);
    int				ret;
    struct ppp_link	*link;
    mbuf_t			m;

    // need to remove all ref to ifnet in link structures
    TAILQ_FOREACH(link, &wan->link_head, lk_bdl_next) {
        // do we need a free function ?
        link->lk_ifnet = 0;
    }

    ppp_comp_close(wan);

    // detach protocols when detaching interface, just in case pppd forgot... 

	lck_mtx_unlock(ppp_domain_mutex);
    ppp_ipv6_detach(ifp, PF_INET6);
    ppp_ip_detach(ifp, PF_INET);
	lck_mtx_lock(ppp_domain_mutex);	
	
    if (wan->vjcomp) {
	FREE(wan->vjcomp, M_TEMP);
	wan->vjcomp = 0;
    }

	wan->state |= PPP_IF_STATE_DETACHING;
	lck_mtx_unlock(ppp_domain_mutex);
    ret = ifnet_detach(ifp);
	if (ret) {
		wan->state &= ~PPP_IF_STATE_DETACHING;
		lck_mtx_lock(ppp_domain_mutex);
		return KERN_FAILURE;
	}
	
	lck_mtx_lock(wan->mtx);
	/* interface release is in progress, wait for callback */
	if (wan->state & PPP_IF_STATE_DETACHING)
		msleep(ifp, wan->mtx, PZERO+1, 0, 0);
	lck_mtx_unlock(wan->mtx);

	//sleep(ifp, PZERO+1);
	lck_mtx_lock(ppp_domain_mutex);
	
    do {
        m = ppp_dequeue(&wan->sndq);
        mbuf_freem(m);
    } while (m);

	lck_mtx_unlock(ppp_domain_mutex);
    ifnet_release(ifp);
	lck_mtx_lock(ppp_domain_mutex);
    TAILQ_REMOVE(&ppp_if_head, wan, next);
	lck_mtx_free(wan->mtx, ppp_if_lck_grp);
    FREE(wan, M_TEMP);

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_if_attachclient(u_short unit, void *host, ifnet_t *ifp)
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
void ppp_if_detachclient(ifnet_t ifp, void *host)
{
    struct ppp_if  	*wan = ifnet_softc(ifp);

    if (wan->host) {
        if (wan->host == host)
            wan->host = 0;
        wan->nbclients--;
        if (!wan->nbclients)
            ppp_if_detach(ifp);
    }
}

/* -----------------------------------------------------------------------------
find a the unit number in the interface list
----------------------------------------------------------------------------- */
struct ppp_if *ppp_if_findunit(u_short unit)
{
    struct ppp_if  	*wan;

    TAILQ_FOREACH(wan, &ppp_if_head, next) {
        if (wan->unit == unit)
            return wan; 
    }
    return NULL;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static errno_t 
ppp_if_set_bpf_tap(ifnet_t ifp, bpf_tap_mode mode,
				bpf_packet_func func)
{
    struct ppp_if 	*wan = ifnet_softc(ifp);

	lck_mtx_lock(ppp_domain_mutex);

    switch (mode) {
        case BPF_MODE_DISABLED:
            wan->bpf_input = wan->bpf_output = NULL;
            break;

        case BPF_MODE_INPUT:
            wan->bpf_input = func;
            break;

        case BPF_MODE_OUTPUT:
			wan->bpf_output = func;
            break;
        
        case BPF_MODE_INPUT_OUTPUT:
            wan->bpf_input = wan->bpf_output = func;
            break;
        default:
            break;
    }
	lck_mtx_unlock(ppp_domain_mutex);
    return 0;
}

/* -----------------------------------------------------------------------------
mbuf debugging
----------------------------------------------------------------------------- */

#ifdef LOGDATA

static void
snprintf_mbuf(char *s, size_t len, mbuf_t m, const char *prefix, const char *suffix)
{
    if (m) {
        if ((mbuf_flags(m) & MBUF_PKTHDR))
            snprintf(s, len, 
                     "%s%p type %u len %u data %p maxlen %u datastart %p next %p flags %x pktlen %u nextpkt %p header %p%s",
                     prefix ? prefix : "",
                     m, mbuf_type(m), mbuf_len(m), mbuf_data(m), mbuf_maxlen(m), mbuf_datastart(m), mbuf_next(m),
                     mbuf_flags(m), mbuf_pkthdr_len(m), mbuf_nextpkt(m), mbuf_pkthdr_header(m),
                     suffix ? suffix : "");
        else
            snprintf(s, len, 
                     "%s%p type %u len %u data %p maxlen %u datastart %p next %p flags %x%s",
                     prefix ? prefix : "",
                     m, mbuf_type(m), mbuf_len(m), mbuf_data(m), mbuf_maxlen(m), mbuf_datastart(m), mbuf_next(m),
                     mbuf_flags(m),
                     suffix ? suffix : "");
    } else
        snprintf(s, len, "%s<NULL>%s", prefix, suffix);
}

static void 
DumpHex(char *line, size_t maxline, unsigned char *buffer, size_t len)
{
    size_t			i;
	int				n = 0;

    for (i = 0; i < len; i += 16) {
        size_t		j;

		if (n > maxline) return;
        n += snprintf(line + n, maxline - n, "%06d: ", i);
        for (j = i; j < len && j < i + 16; j++) {
			if (n > maxline) return;
            n += snprintf(line + n, maxline - n, "%02x ", buffer[j]);
        }
        for (; j < i + 16; j++) {
			if (n > maxline) return;
            n += snprintf(line + n, maxline - n, "   ");
        }
        for (j = i; j < len && j < i + 16; j++) {
            int	c = buffer[j];

            if (c < 0x20 || c > 0x7E)
                c = '.';
			if (n > maxline) return;
            n += snprintf(line + n, maxline - n, "%c", c);
        }
        n += snprintf(line + n, maxline - n, "\n");
    }
}

static void
log_mbuf(ifnet_t ifp, mbuf_t m, const char *msg)
{
    char        mbuf_str[160];
    char        data_str[160];
    
    snprintf_mbuf(mbuf_str, sizeof(mbuf_str), m, NULL, NULL);
    DumpHex(data_str, sizeof(data_str), mbuf_data(m), MIN(mbuf_len(m), 32));
    LOGDBG(ifp, ("ppp%d: %s: %s\n", ifnet_unit(ifp), msg, mbuf_str));
    LOGDBG(ifp, ("%s\n", data_str));
}

#endif /* LOGDATA */

/* -----------------------------------------------------------------------------
called when data are present
----------------------------------------------------------------------------- */
int ppp_if_input(ifnet_t ifp, mbuf_t m, u_int16_t proto, u_int16_t hdrlen)
{    
    struct ppp_if 	*wan = ifnet_softc(ifp);
    int 		inlen, vjlen;
    u_char		*iphdr, *p = mbuf_data(m);
    u_int 		hlen;
    int 		error = ENOMEM;
	struct timespec tv;
	struct		ifnet_stat_increment_param statsinc;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    mbuf_pkthdr_setheader(m, p);		// header point to the protocol header (0x21 or 0x0021)
    mbuf_adj(m, hdrlen);			// the packet points to the real data (0x45)
    p = mbuf_data(m);

    if (wan->sc_flags & SC_DECOMP_RUN) {
        switch (proto) {
            case PPP_COMP:
                if (ppp_comp_decompress(wan, &m) != DECOMP_OK) {
                    LOGDBG(ifp, ("ppp%d: decompression error\n", ifnet_unit(ifp)));
                    goto free;
                }
                p = mbuf_data(m);
                proto = p[0];
                hdrlen = 1;
                if (!(proto & 0x1)) {  // lowest bit set for lowest byte of protocol
                    proto = (proto << 8) + p[1];
                    hdrlen = 2;
                } 
                mbuf_pkthdr_setheader(m, p);// header point to the protocol header (0x21 or 0x0021)
                mbuf_adj(m, hdrlen);	// the packet points to the real data (0x45)
                p = mbuf_data(m);
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
                LOGDBG(ifp, ("ppp%d: VJ structure not allocated\n", ifnet_unit(ifp)));
                goto free;
            }
                
            inlen = mbuf_pkthdr_len(m);

            /*
             * We should make sure the header is contiguous in the mbuf of the compress/decompress routines
             * Do not ask more than the mbuf contains
             */
            if (mbuf_len(m) < MIN(inlen, sizeof(struct ip) + sizeof(struct tcphdr))) {
                mbuf_t      new_m = NULL;
                size_t      offset = 0;
                size_t      len = MIN(inlen, sizeof(struct ip) + sizeof(struct tcphdr));
                
#ifdef LOGDATA
                log_mbuf(ifp, m, proto == PPP_VJC_COMP ? "PPP_VJC_COMP" : "PPP_VJC_UNCOMP");
#endif                    
                if (mbuf_pulldown(m, &offset, len, &new_m) != 0) {
                    LOGDBG(ifp, ("ppp%d: mbuf_pulldown failed\n", ifnet_unit(ifp)));
                    goto end;
                }
                if (new_m != m) {
                    m = new_m;
#ifdef LOGDATA
                    log_mbuf(ifp, m, "pulled-down");
#endif
                }
            }
            if (proto == PPP_VJC_COMP) {

                vjlen = sl_uncompress_tcp_core(p, mbuf_len(m), inlen, TYPE_COMPRESSED_TCP,
                    wan->vjcomp, &iphdr, &hlen);

                if (vjlen <= 0) {
                    LOGDBG(ifp, ("ppp%d: VJ uncompress failed on type PPP_VJC_COMP\n", ifnet_unit(ifp)));
                    goto free;
                }

                // we must move data in the buffer, to add the uncompressed TCP/IP header...
                if (mbuf_trailingspace(m) < (hlen - vjlen)) {
                    LOGDBG(ifp, ("ppp%d: VJ uncompress failed: trailingspace (%d) < hlen (%d) - vjlen (%d)\n", ifnet_unit(ifp),
                        mbuf_trailingspace(m), hlen, vjlen));
                    goto free;
                }
                bcopy(p + vjlen, p + hlen, inlen - vjlen);
                bcopy(iphdr, p, hlen);
				mbuf_setlen(m, mbuf_len(m) + hlen - vjlen);
                mbuf_pkthdr_setlen(m, mbuf_pkthdr_len(m) + hlen - vjlen);
            }
            else {
                vjlen = sl_uncompress_tcp_core(p, mbuf_len(m), inlen, TYPE_UNCOMPRESSED_TCP, 
                    wan->vjcomp, &iphdr, &hlen);

                if (vjlen < 0) {
                    LOGDBG(ifp, ("ppp%d: VJ uncompress failed on type TYPE_UNCOMPRESSED_TCP\n", ifnet_unit(ifp)));
                    goto free;
                }
            }
            *(u_char *)mbuf_pkthdr_header(m) = PPP_IP; // change the protocol, use 1 byte
            proto = PPP_IP;
            //no break;
        case PPP_IP:
            if (wan->npmode[NP_IP] != NPMODE_PASS)
                goto reject;
            if (wan->npafmode[NP_IP] & NPAFMODE_SRC_IN) {
                if (ppp_ip_af_src_in(ifp, mbuf_data(m))) {
                    error = 0;
                    goto free;
                }
            }
            if (wan->npafmode[NP_IP] & NPAFMODE_DHCP_INTERCEPT_SERVER) {
				if (ppp_ip_bootp_server_in(ifp, mbuf_data(m)))
					goto reject;
            }
            if (wan->npafmode[NP_IP] & NPAFMODE_DHCP_INTERCEPT_CLIENT) {
                if (ppp_ip_bootp_client_in(ifp, mbuf_data(m)))
					goto reject;
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
    if (wan->bpf_input) {
        if (mbuf_prepend(&m, 4, MBUF_WAITOK) != 0) {
			bzero(&statsinc, sizeof(statsinc));
			statsinc.errors_in = 1;
			ifnet_stat_increment(ifp, &statsinc);		
            return ENOMEM;
        }
        p = mbuf_data(m);
        *(u_int16_t *)p = htons(0xFF03);
        *(u_int16_t *)(p + 2) = htons(proto);
        (*wan->bpf_input)(ifp, m);
        mbuf_adj(m, 4);
    }

	bzero(&statsinc, sizeof(statsinc));
	statsinc.packets_in = 1;
	statsinc.bytes_in = mbuf_pkthdr_len(m);
	mbuf_pkthdr_setrcvif(m, ifp);
    nanouptime(&tv);
    wan->last_recv = tv.tv_sec;

	lck_mtx_unlock(ppp_domain_mutex);
    ifnet_input(ifp, m, &statsinc);
	lck_mtx_lock(ppp_domain_mutex);
    return 0;
    
reject:

    // unexpected network protocol, prepend the 2 bytes protocol header expected by pppd
	if (mbuf_prepend(&m, 2, MBUF_WAITOK) != 0) {
		bzero(&statsinc, sizeof(statsinc));
		statsinc.errors_in = 1;
		ifnet_stat_increment(ifp, &statsinc);		
		return ENOMEM;
	}
	p = mbuf_data(m);
	*(u_int16_t *)p = htons(proto);
    ppp_proto_input(wan->host, m);
    return 0;
    
free:
    mbuf_freem(m);
end:
	bzero(&statsinc, sizeof(statsinc));
	statsinc.errors_in = 1;
	ifnet_stat_increment(ifp, &statsinc);		
    return error;
}

/* -----------------------------------------------------------------------------
This gets called when the interface is freed
(if dlil_if_detach has returned DLIL_WAIT_FOR_FREE)
----------------------------------------------------------------------------- */
void ppp_if_if_free(ifnet_t ifp)
{
    struct ppp_if  	*wan = ifnet_softc(ifp);

	lck_mtx_lock(wan->mtx);
	wan->state &= ~PPP_IF_STATE_DETACHING;
	lck_mtx_unlock(wan->mtx);
    wakeup(ifp);
}

/* -----------------------------------------------------------------------------
Process an ioctl request to the ppp interface
----------------------------------------------------------------------------- */
int ppp_if_control(ifnet_t ifp, u_long cmd, void *data)
{
    struct ppp_if 	*wan = ifnet_softc(ifp);
    int 		error = 0, npx;
    u_int16_t		mru, flags16;
    u_int32_t		flags;
    u_int32_t		t;
    struct npioctl 	*npi;
    struct npafioctl 	*npafi;
	struct timespec tv;	

    //LOGDBG(ifp, ("ppp_if_control, (ifnet = %s%d), cmd = 0x%x\n", ifp->if_name, ifp->if_unit, cmd));

	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
	
    switch (cmd) {
	case PPPIOCSDEBUG:
            flags = *(int *)data;
            LOGDBG(ifp, ("ppp_if_control: PPPIOCSDEBUG (level = 0x%x)\n", flags));
            flags16 = 0;  
            if (flags & 1) flags16 |= IFF_DEBUG;	// general purpose debugging
            if (flags & 2) flags16 |= PPP_LOG_INPKT;	// trace all packets in
            if (flags & 4) flags16 |= PPP_LOG_OUTPKT;	// trace all packets out
			ifnet_set_flags(ifp, flags16, IFF_DEBUG + PPP_LOG_INPKT + PPP_LOG_OUTPKT);
            break;

	case PPPIOCGDEBUG:
			flags16 = ifnet_flags(ifp);
            flags = 0;
            if (flags16 & IFF_DEBUG) flags |= 1;
            if (flags16 & PPP_LOG_INPKT) flags |= 2;
            if (flags16 & PPP_LOG_OUTPKT) flags |= 4;
            *(int *)data = flags;
            LOGDBG(ifp, ("ppp_if_control: PPPIOCGDEBUG (level = 0x%x)\n", flags));
            break;

	case PPPIOCSMRU:
            LOGDBG(ifp, ("ppp_if_control: PPPIOCSMRU\n"));
            mru = *(int *)data;
            wan->mru = mru;
            break;

	case PPPIOCSFLAGS:
            flags = *(int *)data & SC_MASK;
            LOGDBG(ifp, ("ppp_if_control: PPPIOCSFLAGS, old flags = 0x%x new flags = 0x%x, \n", wan->sc_flags, (wan->sc_flags & ~SC_MASK) | flags));
            wan->sc_flags = (wan->sc_flags & ~SC_MASK) | flags;
            break;

	case PPPIOCGFLAGS:
            LOGDBG(ifp, ("ppp_if_control: PPPIOCGFLAGS\n"));
            *(int *)data = wan->sc_flags;
            break;

	case PPPIOCSCOMPRESS32:
	case PPPIOCSCOMPRESS64:
            error = ppp_comp_setcompressor(wan, data);
            break;

	case PPPIOCGUNIT:
            LOGDBG(ifp, ("ppp_if_control: PPPIOCGUNIT\n"));
            *(int *)data = ifnet_unit(ifp);
            break;

	case PPPIOCGIDLE:
            LOGDBG(ifp, ("ppp_if_control: PPPIOCGIDLE\n"));
			nanouptime(&tv);
			t = tv.tv_sec;
            ((struct ppp_idle *)data)->xmit_idle = t - wan->last_xmit;
            ((struct ppp_idle *)data)->recv_idle = t - wan->last_recv;
            break;

        case PPPIOCSMAXCID:
            LOGDBG(ifp, ("ppp_if_control: PPPIOCSMAXCID\n"));
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
            LOGDBG(ifp, ("ppp_if_control: PPPIOCSNPMODE/PPPIOCGNPMODE\n"));
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
            LOGDBG(ifp, ("ppp_if_control: PPPIOCSNPAFMODE/PPPIOCGNPAFMODE\n"));
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
            LOGDBG(ifp, ("ppp_if_control: unknown ioctl\n"));
            error = EINVAL;
	}

    return error;
}

/* -----------------------------------------------------------------------------
Process an ioctl request to the ppp interface
----------------------------------------------------------------------------- */
errno_t ppp_if_ioctl(ifnet_t ifp, u_long cmd, void *data)
{
    //struct ppp_if 	*wan = ifp->if_softc;
    struct ifreq 	*ifr = (struct ifreq *)data;
    int 		error = 0;
    struct ppp_stats 	*psp;
	struct ifnet_stats_param statspar;

    //LOGDBG(ifp, ("ppp_if_ioctl, cmd = 0x%x\n", cmd));
	
	lck_mtx_lock(ppp_domain_mutex);

    switch (cmd) {

        case SIOCSIFFLAGS:
            LOGDBG(ifp, ("ppp_if_ioctl: cmd = SIOCSIFFLAGS\n"));
            // even if this case does nothing, it must be there to return 0
            //if ((ifp->if_flags & IFF_RUNNING) == 0)
            //    ifp->if_flags &= ~IFF_UP;
            break;

        case SIOCSIFADDR:
        case SIOCAIFADDR:
            LOGDBG(ifp, ("ppp_if_ioctl: cmd = SIOCSIFADDR/SIOCAIFADDR\n"));
            // dlil protocol module already took care of the ioctl
            break;

        case SIOCADDMULTI:
        case SIOCDELMULTI:
            LOGDBG(ifp, ("ppp_if_ioctl: cmd = SIOCADDMULTI/SIOCDELMULTI\n"));
            break;

        case SIOCDIFADDR:
            LOGDBG(ifp, ("ppp_if_ioctl: cmd = SIOCDIFADDR\n"));
            break;

        case SIOCSIFDSTADDR:
            LOGDBG(ifp, ("ppp_if_ioctl: cmd = SIOCSIFDSTADDR\n"));
            break;

	case SIOCGPPPSTATS:
            LOGDBG(ifp, ("ppp_if_ioctl, SIOCGPPPSTATS\n"));
            psp = &((struct ifpppstatsreq *) data)->stats;
            bzero(psp, sizeof(*psp));
			ifnet_stat(ifp, &statspar); 
			/* 
				XXX ppp pcounters are only 32 bits.
				need to implement a second ioctl  
				for 64 bits conters
			*/
            psp->p.ppp_ibytes = statspar.bytes_in;
            psp->p.ppp_obytes = statspar.bytes_out;
            psp->p.ppp_ipackets = statspar.packets_in;
            psp->p.ppp_opackets = statspar.packets_out;
            psp->p.ppp_ierrors = statspar.errors_in;
            psp->p.ppp_oerrors = statspar.errors_out;

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
            LOGDBG(ifp, ("ppp_if_ioctl, SIOCSIFMTU\n"));
            // should we check the minimum MTU for all channels attached to that interface ?
            if (ifr->ifr_mtu > PPP_MTU)
                error = EINVAL;
            else
                ifnet_set_mtu(ifp, ifr->ifr_mtu);
            break;

	default:
            LOGDBG(ifp, ("ppp_if_ioctl, unknown ioctl, cmd = 0x%x\n", cmd));
            error = EOPNOTSUPP;
	}
	lck_mtx_unlock(ppp_domain_mutex);
    return error;
}

/* -----------------------------------------------------------------------------
This gets called at splnet from if_ppp.c at various times
when there is data ready to be sent
----------------------------------------------------------------------------- */
errno_t ppp_if_output(ifnet_t ifp, mbuf_t m)
{
    struct ppp_if 	*wan = ifnet_softc(ifp);
    int 		error = 0;
    u_int16_t		proto;
    enum NPmode		mode;
    enum NPAFmode	afmode;
    char		*p;
	struct timespec tv;	
	struct		ifnet_stat_increment_param statsinc;
	
	lck_mtx_lock(ppp_domain_mutex);
	    
	// clear any flag that can confuse the underlying driver
	mbuf_setflags(m, mbuf_flags(m) & ~(M_BCAST + M_MCAST));

    proto = ntohs(*(u_int16_t*)mbuf_data(m));
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
		mbuf_pullup(&m, sizeof(struct ip) + 2);
		if (m == 0) {
			bzero(&statsinc, sizeof(statsinc));
			statsinc.errors_out = 1;
			ifnet_stat_increment(ifp, &statsinc);		
            return ENOBUFS;
		}
        p = mbuf_data(m);
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
	lck_mtx_unlock(ppp_domain_mutex);
    if (wan->bpf_output) {
        if (mbuf_prepend(&m, 2, MBUF_WAITOK) != 0) {
			bzero(&statsinc, sizeof(statsinc));
			statsinc.errors_out = 1;
			ifnet_stat_increment(ifp, &statsinc);		
            return ENOBUFS;
        }
        *(u_int16_t*)mbuf_data(m) = htons(0xFF03);
		(*wan->bpf_output)(ifp, m);
        mbuf_adj(m, 2);
    }
	lck_mtx_lock(ppp_domain_mutex);

    // Update interface statistics.
	ifnet_touch_lastchange(ifp);
	nanouptime(&tv);
	wan->last_xmit = tv.tv_sec;
	bzero(&statsinc, sizeof(statsinc));
	statsinc.bytes_out = mbuf_pkthdr_len(m) - 2; // don't count protocol header;
	statsinc.packets_out = 1;
	ifnet_stat_increment(ifp, &statsinc);		

    if (wan->sc_flags & SC_LOOP_TRAFFIC) {
        ppp_proto_input(wan->host, m);
        lck_mtx_unlock(ppp_domain_mutex);
        return 0;
    }
        
    error = ppp_if_send(ifp, m);
	lck_mtx_unlock(ppp_domain_mutex);
    return error;

bad:
    mbuf_freem(m);
	bzero(&statsinc, sizeof(statsinc));
	statsinc.errors_out = 1;
	ifnet_stat_increment(ifp, &statsinc);		
	lck_mtx_unlock(ppp_domain_mutex);
    return error;
}

/* -----------------------------------------------------------------------------
add protocol function
called from dlil when a network protocol is attached for an
interface from that family (i.e ip is attached through ppp_attach_ip)
----------------------------------------------------------------------------- */
int  ppp_if_add_proto(ifnet_t ifp, protocol_family_t protocol_family,
			const struct ifnet_demux_desc *demux_list, u_int32_t demux_count)
{        
    LOGDBG(ifp, ("ppp_if_add_proto = %d, ifp = %p\n", protocol_family, ifp));
    
    switch (protocol_family) {
        case PF_INET:
        case PF_INET6:
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
int  ppp_if_del_proto(ifnet_t ifp, protocol_family_t protocol_family)
{

    LOGDBG(ifp, ("ppp_if_del_proto, ifp = %p\n", ifp));
    
    return 0;
}

/* -----------------------------------------------------------------------------
demux function
----------------------------------------------------------------------------- */
int ppp_if_demux(ifnet_t ifp, mbuf_t m, char *frame_header,
                  protocol_family_t *protocol_family)
{
    u_int16_t 		proto;

    proto = frame_header[0];
    if (!proto & 0x1) {  // lowest bit set for lowest byte of protocol
        proto = (proto << 8) + frame_header[1];
    } 

    switch (proto) {
        case PPP_IP:
			// We could check pppfam->ip_attached, but dlil will handle case where it isn't
			*protocol_family = PF_INET;
            break;
        case PPP_IPV6:
			// We could check pppfam->ipv6_attached, but dlil will handle case where it isn't
			*protocol_family = PF_INET6;
            break;
        default :
            LOGDBG(ifp, ("ppp_fam_demux, ifp = %p, bad proto = 0x%x\n", ifp, proto));
            return ENOENT;	// should never happen
    }
    
    return 0;
}

/* -----------------------------------------------------------------------------
a network packet needs to be send through the interface.
add the ppp header to the packet (as a network interface, we only worry
about adding our protocol number)
----------------------------------------------------------------------------- */
errno_t ppp_if_frameout(ifnet_t ifp, mbuf_t *m0,
                     const struct sockaddr *ndest, const char *edst, const char *ppp_type)
{
	struct		ifnet_stat_increment_param statsinc;

    if (mbuf_prepend(m0, 2, MBUF_DONTWAIT) != 0) {
        LOGDBG(ifp, ("ppp_fam_ifoutput : no memory for transmit header\n"));
		bzero(&statsinc, sizeof(statsinc));
		statsinc.errors_out = 1;
		ifnet_stat_increment(ifp, &statsinc);		
        return EJUSTRETURN;	// just return, because the buffer was freed in m_prepend
    }

    // place protocol number at the beginning of the mbuf
    *(u_int16_t*)mbuf_data(*m0) = htons(*(u_int16_t *)ppp_type);
    
    return 0;
}

/* -----------------------------------------------------------------------------
 * Connect a PPP channel to a PPP interface unit.
----------------------------------------------------------------------------- */
int ppp_if_attachlink(struct ppp_link *link, int unit)
{
    struct ppp_if 	*wan;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    if (link->lk_ifnet)	// already attached
        return EINVAL;

    wan = ppp_if_findunit(unit);
    if (!wan)
	return EINVAL;

	ifnet_set_flags(wan->net, IFF_RUNNING, IFF_RUNNING);
    ifnet_set_baudrate(wan->net, ifnet_baudrate(wan->net) + link->lk_baudrate);

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
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    if (!ifp)
        return EINVAL; // link already detached

    wan = ifp->if_softc;
    
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
int ppp_if_send(ifnet_t ifp, mbuf_t m)
{
    struct ppp_if 	*wan = ifnet_softc(ifp);
    u_int16_t		proto = ntohs(*(u_int16_t*)mbuf_data(m));	// always the 2 first bytes
	struct			ifnet_stat_increment_param statsinc;
	int				error = 0;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
        
    if (ppp_qfull(&wan->sndq)) {
        ppp_drop(&wan->sndq);
		bzero(&statsinc, sizeof(statsinc));
		statsinc.errors_out = 1;
		ifnet_stat_increment(ifp, &statsinc);		
        mbuf_freem(m);
        return ENOBUFS;
    }

    switch (proto) {
        case PPP_IP:
            // see if we can compress it
            if ((wan->sc_flags & SC_COMP_TCP) && wan->vjcomp) {
                mbuf_t		mp = m;
                struct ip 	*ip = mbuf_data(m) + 2;
                int 		vjtype, len;
                
                // skip mbuf, in case the ppp header and ip header are not in the same mbuf
                if (mbuf_len(mp) <= 2) {
                    mp = mbuf_next(mp);
                    if (!mp)
                        break;
                    ip = mbuf_data(mp);
                }
                // this code assumes the IP/TCP header is in one non-shared mbuf 
                if (ip->ip_p == IPPROTO_TCP) {
                    vjtype = sl_compress_tcp(mp, ip, wan->vjcomp, !(wan->sc_flags & SC_NO_TCP_CCID));
                    switch (vjtype) {
                        case TYPE_UNCOMPRESSED_TCP:
                            *(u_int16_t*)mbuf_data(m) = htons(PPP_VJC_UNCOMP); // update protocol
                            break;
                        case TYPE_COMPRESSED_TCP:
                            *(u_int16_t*)mbuf_data(m) = htons(PPP_VJC_COMP); // header has moved, update protocol
                        break;
                    }
                    // adjust packet len
                    len = 0;
                    for (mp = m; mp != 0; mp = mbuf_next(mp))
                        len += mbuf_len(mp);
                    mbuf_pkthdr_setlen(m, len);
                }
            }
            break;
        case PPP_CCP:
            mbuf_adj(m, 2);
            ppp_comp_ccp(wan, m, 0);
            mbuf_prepend(&m, 2, MBUF_DONTWAIT);
            break;
    }

    if (wan->sc_flags & SC_COMP_RUN) {

        if (ppp_comp_compress(wan, &m) == COMP_OK) {
            if (mbuf_prepend(&m, 2, MBUF_DONTWAIT) != 0) {
				bzero(&statsinc, sizeof(statsinc));
				statsinc.errors_out = 1;
				ifnet_stat_increment(ifp, &statsinc);		
                return ENOBUFS;
            }
            *(u_int16_t*)mbuf_data(m) = htons(PPP_COMP); // update protocol
        } 
    } 

    if (wan->sndq.len) {
        ppp_enqueue(&wan->sndq, m);
    }
    else 
		error = ppp_if_xmit(ifp, m);
    
    return error;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_if_xmit(ifnet_t ifp, mbuf_t m)
{
    struct ppp_if 	*wan = ifnet_softc(ifp);
    struct ppp_link	*link;
    int 		error = 0, len;
	struct		ifnet_stat_increment_param statsinc;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
            
    if (m == 0)
        m = ppp_dequeue(&wan->sndq);

    while (m) {

        link = TAILQ_FIRST(&wan->link_head);
        if (link == 0) {
            LOGDBG(ifp, ("ppp%d: Trying to send data with link detached\n", ifnet_unit(ifp)));
            // just flush everything
            error = ENXIO;
			goto flush;
        }
    
        if (link->lk_flags & SC_HOLD) {
            // should try next link
            mbuf_freem(m);
            m = ppp_dequeue(&wan->sndq);
            continue;
        }

        if (link->lk_flags & (SC_XMIT_BUSY | SC_XMIT_FULL)) {
            // should try next link
            ppp_prepend(&wan->sndq, m);
            return 0;
        }

        // get the len before we send the packet, 
        // we can not assume the state of the mbuf when we return
        len = mbuf_len(m);

        // since we tested the lk_flags, ppp_link_send should not failed
        // except if there is a dramatic error
        link->lk_flags |= SC_XMIT_BUSY;
        error = ppp_link_send(link, m);
        link->lk_flags &= ~SC_XMIT_BUSY;
        if (error) {
            // packet has been freed by link lower layer
			m = 0;
			goto flush;
        }
            
         m = ppp_dequeue(&wan->sndq);
    }
     
    return 0;
	
flush:

	ifnet_touch_lastchange(ifp);
	do {
		bzero(&statsinc, sizeof(statsinc));
		statsinc.errors_out = 1;
		ifnet_stat_increment(ifp, &statsinc);
		if (m)
			mbuf_freem(m);
		m = ppp_dequeue(&wan->sndq);
	}
	while (m);
	return error;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_if_error(ifnet_t ifp)
{
    struct ppp_if 	*wan = ifnet_softc(ifp);
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
        
    // reset vj compression
    if (wan->vjcomp) {
	sl_uncompress_tcp(NULL, 0, TYPE_ERROR, wan->vjcomp);
    }
}
