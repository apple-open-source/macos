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


#ifndef _IF_PPPLINK_H_
#define _IF_PPPLINK_H_

#define LKNAMSIZ	IFNAMSIZ

struct link_data {
	/* generic interface information */
	u_int8_t	type;		/* PPPoE, PPPoA, Async Line Disc, ... */
	u_int8_t	hdrlen;		/* media header length */
	u_int16_t	mtu;		/* maximum transmission unit */
	u_int16_t	mru;		/* maximum receive unit */
        u_int32_t 	support;	/* misc support flags */
	u_int32_t	baudrate;	/* linespeed */
	u_int32_t	flags;		/* configuration flags */
	/* volatile statistics */
	u_int32_t	ipackets;	/* packets received on link */
	u_int32_t	ierrors;	/* input errors on link */
	u_int32_t	opackets;	/* packets sent on link */
	u_int32_t	oerrors;	/* output errors on link */
	u_int32_t	ibytes;		/* total number of octets received */
	u_int32_t	obytes;		/* total number of octets sent */
	u_int32_t	iqdrops;	/* dropped on input, this link */
        time_t		last_xmit; 	/* last proto packet sent on this link */
        time_t		last_recv; 	/* last proto packet received on this link */
};

struct iflink {
    /* information filled by the ppp driver, yet accessible by the link driver */
    TAILQ_ENTRY(iflink) lk_next; 	/* all struct iflink are chained */
    TAILQ_ENTRY(iflink) bdl_next; 	/* all struct iflink for an ifnet bundle are chained */
    u_int16_t		lk_index;	/* unit number, given by ppp at register time */
    struct ifnet	*ifnet;		/* associated ppp ifnet structure */
    void 		*ppp_cookie;	/* ppp private data */
//   ?? struct ifqueue 	link_snd;	/* link output queue */
//   ?? struct ifqueue 	link_fastsnd;	/* link priority output queue */

    /* information provided by the the link driver */
    u_char		*lk_name;	/* name, e.g. ``async tty'' or ``pppoe'', [name+unit <= LKNAMSIZ] */
    u_int16_t		lk_unit;	/* sub unit for link driver */
    u_int32_t		lk_state;	/* current state bits */
    struct link_data	lk_data;	/* misc link data information */

    int	(*lk_output)(struct iflink *iflink, struct mbuf *m);
    int	(*lk_ioctl)(struct iflink *iflink, u_int32_t cmd, void *data);    

    void 		*reserved1;	/* reserved for future use*/
    void 		*reserved2;	/* reserved for future use*/
};


int ppp_link_attach(struct iflink *iflink);
int ppp_link_detach(struct iflink *link);

int ppp_link_input(struct iflink *link, struct mbuf *m);
int ppp_link_event(struct iflink *link, u_int32_t event, void *data);


/* name used for the link */
/* to avoid confict, a different implementation should use a different name */
//#define APPLE_PPP_NAME_SERIAL	"serial"
// belongs to the driver header ?

/* links must specify their type */
#define PPP_TYPE_SERIAL		0	/* Asynchonous Line Discipline */
#define PPP_TYPE_PPPoE		2	/* PPP over Ethernet */
#define PPP_TYPE_PPPoA		3	/* PPP over ATM */
#define PPP_TYPE_PPTP		4	/* Point-to-Point Tunneling Protocol */
#define PPP_TYPE_L2TP		5	/* Layer 2 Tunneling Protocol */
#define PPP_TYPE_ISDN		6	/* Integrated Service Digital Network */


/* values for events */

#define PPPLINK_EVT_XMIT_OK 	1
#define PPPLINK_EVT_INPUTERROR	2

/* values for link support */
// set by the driver to inform the mux that the driver doesn't want ALLSTATION/UI even if not negociated
// mux need to remove address/control field prior to send packet to the driver
// mux must expect packets from the driver without thos fields
#define PPPLINK_DEL_AC		0x00000001		/* link doesn't want address and control bytes */
#define PPPLINK_ASYNC		0x00000002		/* link does asynchronous framing */
#define PPPLINK_ERRORDETECT	0x00000004		/* link does error detection */

/* values for link state */
#define PPPLINK_STATE_XMIT_FULL	0x00000001		/* link doesn't want data anymore (associated with PPPLINK_EVT_XMIT_OK)  */



/* Macro facilities */

#define LKIFNET(lk)		(((struct iflink *)lk)->ifnet)
#define LKNAME(lk) 		(((struct iflink*)lk)->lk_name)
#define LKUNIT(lk) 		(((struct iflink*)lk)->lk_unit)

#define LKIFFDEBUG(lk) 		(LKIFNET(lk) ? LKIFNET(lk)->if_flags & IFF_DEBUG : 0 )
#define LKIFNAME(lk) 		(LKIFNET(lk) ? LKIFNET(lk)->if_name : "???")
#define LKIFUNIT(lk) 		(LKIFNET(lk) ? LKIFNET(lk)->if_unit : 0)

#define LOGLKDBG(lk, text) \
    if (LKIFNET(lk) && (LKIFNET(lk)->if_flags & IFF_DEBUG)) {	\
        log text; 		\
    }

#endif /* _IF_PPPLINK_H_ */

