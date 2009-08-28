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


#ifndef _IF_PPP_LINK_H_
#define _IF_PPP_LINK_H_


/* links must specify their type */
#define PPP_TYPE_SERIAL		0		/* Asynchonous Line Discipline */
#define PPP_TYPE_SYNCSERIAL	1		/* Synchonous Line Discipline */
#define PPP_TYPE_PPPoE		2		/* PPP over Ethernet */
#define PPP_TYPE_PPPoA		3		/* PPP over ATM */
#define PPP_TYPE_PPTP		4		/* Point-to-Point Tunneling Protocol */
#define PPP_TYPE_L2TP		5		/* Layer 2 Tunneling Protocol */
#define PPP_TYPE_OTHER		0xFFF0		/* Undefined PPP type */


#ifdef KERNEL

/* values for events */
#define PPP_LINK_EVT_XMIT_OK 	1
#define PPP_LINK_EVT_INPUTERROR	2


/* values for link support */
#define PPP_LINK_DEL_AC		0x00000001	/* link doesn't want address and control bytes */
#define PPP_LINK_ASYNC		0x00000002	/* link does asynchronous framing */
#define PPP_LINK_ERRORDETECT	0x00000004	/* link does error detection */
#define PPP_LINK_OOB_QUEUE	0x00000008	/* link support out-of-band priority queue */


/* miscellaneous debug flags */
#define PPP_LOG_INPKT 		IFF_LINK0
#define PPP_LOG_OUTPKT 		IFF_LINK1


/* attachment structure created by the link driver */
struct ppp_link {
    /* information filled in by the ppp driver, link driver shouldn't modify these fields */
    TAILQ_ENTRY(ppp_link) lk_next; 		/* all struct ppp_link are chained */
    TAILQ_ENTRY(ppp_link) lk_bdl_next; 		/* all struct ppp_link for an ifnet bundle are chained */
    u_int16_t		lk_index;		/* unit number, given by ppp at register time */
    ifnet_t			lk_ifnet;		/* associated ppp ifnet structure */
    void 		*lk_ppp_private;	/* ppp private data */

    /* link information, provided by the link driver when attaching */
    u_char		*lk_name;		/* name, e.g. ``async tty'' or ``pppoe'', [name+unit <= IFNAMSIZ] */
    u_int16_t		lk_unit;		/* sub unit for link driver */
    u_int8_t		lk_type;		/* PPPoE, PPPoA, Async Line Disc, ... */
    u_int8_t		lk_hdrlen;		/* media header length */
    u_int16_t		lk_mtu;			/* maximum transmission unit */
    u_int16_t		lk_mru;			/* maximum receive unit */
    u_int32_t 		lk_support;		/* misc support flags */
    u_int32_t		lk_baudrate;		/* line speed */
    u_int32_t		lk_flags;		/* configuration flags, shared by the driver and ppp */
   
    /* link specific functions, called by the ppp driver */
    int			(*lk_output)		/* output function */
                            (struct ppp_link *link, mbuf_t m);
    int			(*lk_ioctl)		/* control function */
                            (struct ppp_link *link, u_long cmd, void *data);    

    /* statistics and state information, updated by the link driver */
    u_int32_t		lk_reserved0;		/* reserved for future use */
    u_int32_t		lk_ipackets;		/* packets received on link */
    u_int32_t		lk_ierrors;		/* input errors on link */
    u_int32_t		lk_opackets;		/* packets sent on link */
    u_int32_t		lk_oerrors;		/* output errors on link */
    u_int32_t		lk_ibytes;		/* total number of octets received */
    u_int32_t		lk_obytes;		/* total number of octets sent */
    time_t		lk_last_xmit; 		/* last packet sent on this link */
    time_t		lk_last_recv; 		/* last packet received on this link */

    /* private data pointer for the link driver */
    void 		*lk_private;		/* link private data */

    /* reserved for future use */
    void 		*lk_reserved1;		/* reserved for future use */
    void 		*lk_reserved2;		/* reserved for future use */
    void 		*lk_reserved3;		/* reserved for future use */
    void 		*lk_reserved4;		/* reserved for future use */
};


int ppp_link_attach(struct ppp_link *link);
int ppp_link_detach(struct ppp_link *link);

int ppp_link_input(struct ppp_link *link, mbuf_t m);
int ppp_link_event(struct ppp_link *link, u_int32_t event, void *data);

void ppp_link_logmbuf(struct ppp_link *link, char *msg, mbuf_t m);

#endif /* KERNEL */

#endif /* _IF_PPP_LINK_H_ */

