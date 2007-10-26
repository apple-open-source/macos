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

#ifndef _PPP_IF_H_
#define _PPP_IF_H_

/*
 * Network protocols we support.
 */
#define NP_IP	0		/* Internet Protocol V4 */
#define NP_IPV6	1		/* Internet Protocol V6 */
//#define NP_IPX	2		/* IPX protocol */
//#define NP_AT	3		/* Appletalk protocol */
#define NUM_NP	2		/* Number of NPs. */

/*
 * State of the interface.
 */
#define PPP_IF_STATE_DETACHING	1

struct ppp_if {
    /* first, the ifnet structure... */
    ifnet_t				net;		/* network-visible interface */

    /* administrative info */
    TAILQ_ENTRY(ppp_if) next;
    void				*host;		/* first client structure */
    u_int8_t			nbclients;	/* nb clients attached */
	u_int8_t			state;		/* state of the interface */
	lck_mtx_t			*mtx;		/* interface mutex */
	u_short				unit;		/* unit number (same as in ifnet_t) */
	
    /* ppp data */
    u_int16_t			mru;		/* max receive unit */
    TAILQ_HEAD(, ppp_link)  link_head; 	/* list of links attached to this interface */
    u_int8_t			nblinks;	/* # links currently attached */
    mbuf_t				outm;		/* mbuf currently being output */
    time_t				last_xmit; 	/* last proto packet sent on this interface */
    time_t				last_recv; 	/* last proto packet received on this interface */
    u_int32_t			sc_flags;	/* ppp private flags */
    struct slcompress	*vjcomp; 	/* vjc control buffer */
    enum NPmode			npmode[NUM_NP];	/* what to do with each net proto */
    enum NPAFmode		npafmode[NUM_NP];/* address filtering for each net proto */
	struct pppqueue		sndq;		/* send queue */
	bpf_packet_func		bpf_input;	/* bpf input function */
	bpf_packet_func		bpf_output;	/* bpf output function */
	
    /* data compression */
    void				*xc_state;	/* send compressor state */
    struct ppp_comp		*xcomp;		/* send compressor structure */
    void				*rc_state;	/* send compressor state */
    struct ppp_comp		*rcomp;		/* send compressor structure */

	/* network protocols data */
    int					ip_attached;
    struct in_addr		ip_src;
    struct in_addr		ip_dst;
    int					ipv6_attached;
    ifnet_t				lo_ifp;		/* loopback interface */
};


/*
 * Bits in sc_flags: SC_NO_TCP_CCID, SC_CCP_OPEN, SC_CCP_UP, SC_LOOP_TRAFFIC,
 * SC_MULTILINK, SC_MP_SHORTSEQ, SC_MP_XSHORTSEQ, SC_COMP_TCP, SC_REJ_COMP_TCP.
 */
#define SC_FLAG_BITS	(SC_NO_TCP_CCID|SC_CCP_OPEN|SC_CCP_UP|SC_LOOP_TRAFFIC \
			 |SC_MULTILINK|SC_MP_SHORTSEQ|SC_MP_XSHORTSEQ \
			 |SC_COMP_TCP|SC_REJ_COMP_TCP)


int ppp_if_init();
int ppp_if_dispose();
int ppp_if_attach(u_short *unit);
int ppp_if_attachclient(u_short unit, void *host, ifnet_t *ifp);
void ppp_if_detachclient(ifnet_t ifp, void *host);

int ppp_if_input(ifnet_t ifp, mbuf_t m, u_int16_t proto, u_int16_t hdrlen);
int ppp_if_control(ifnet_t ifp, u_long cmd, void *data);
int ppp_if_attachlink(struct ppp_link *link, int unit);
int ppp_if_detachlink(struct ppp_link *link);
int ppp_if_send(ifnet_t ifp, mbuf_t m);
void ppp_if_error(ifnet_t ifp);
int ppp_if_xmit(ifnet_t ifp, mbuf_t m);



#define APPLE_PPP_NAME	"ppp"



#endif /* _PPP_IF_H_ */
