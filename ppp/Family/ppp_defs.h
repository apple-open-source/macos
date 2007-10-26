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
/*
 * ppp_defs.h - PPP definitions.
 *
 * Copyright (c) 1994 The Australian National University.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, provided that the above copyright
 * notice appears in all copies.  This software is provided without any
 * warranty, express or implied. The Australian National University
 * makes no representations about the suitability of this software for
 * any purpose.
 *
 * IN NO EVENT SHALL THE AUSTRALIAN NATIONAL UNIVERSITY BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
 * THE AUSTRALIAN NATIONAL UNIVERSITY HAVE BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * THE AUSTRALIAN NATIONAL UNIVERSITY SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE AUSTRALIAN NATIONAL UNIVERSITY HAS NO
 * OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
 * OR MODIFICATIONS.
 */

#ifndef _PPP_DEFS_H_
#define _PPP_DEFS_H_

/*
 * The basic PPP frame.
 */
#define PPP_HDRLEN	4	/* octets for standard ppp header */
#define PPP_FCSLEN	2	/* octets for FCS */
#define PPP_MRU		1500	/* default MRU = max length of info field */

#define PPP_ADDRESS(p)	(((u_int8_t *)(p))[0])
#define PPP_CONTROL(p)	(((u_int8_t *)(p))[1])
#define PPP_PROTOCOL(p)	((((u_int8_t *)(p))[2] << 8) + ((u_int8_t *)(p))[3])

/*
 * Significant octet values.
 */
#define	PPP_ALLSTATIONS	0xff	/* All-Stations broadcast address */
#define	PPP_UI		0x03	/* Unnumbered Information */
#define	PPP_FLAG	0x7e	/* Flag Sequence */
#define	PPP_ESCAPE	0x7d	/* Asynchronous Control Escape */
#define	PPP_TRANS	0x20	/* Asynchronous transparency modifier */

/*
 * Protocol field values.
 */
#define PPP_IP		0x21	/* Internet Protocol */
#define PPP_AT		0x29	/* AppleTalk Protocol */
#define PPP_IPX		0x2b	/* IPX protocol */
#define	PPP_VJC_COMP	0x2d	/* VJ compressed TCP */
#define	PPP_VJC_UNCOMP	0x2f	/* VJ uncompressed TCP */
#define PPP_MP		0x3d	/* Multilink protocol */
#define PPP_IPV6	0x57	/* Internet Protocol Version 6 */
#define PPP_COMPFRAG	0xfb	/* fragment compressed below bundle */
#define PPP_COMP	0xfd	/* compressed packet */
#define PPP_ACSP	0x235	/* Apple Client Server Protocol */
#define PPP_IPCP	0x8021	/* IP Control Protocol */
#define PPP_ATCP	0x8029	/* AppleTalk Control Protocol */
#define PPP_IPXCP	0x802b	/* IPX Control Protocol */
#define PPP_IPV6CP	0x8057	/* IPv6 Control Protocol */
#define PPP_CCPFRAG	0x80fb	/* CCP at link level (below MP bundle) */
#define PPP_CCP		0x80fd	/* Compression Control Protocol */
#define PPP_ECPFRAG     0x8055  /* ECP at link level (below MP bundle) */
#define PPP_ECP         0x8053  /* Encryption Control Protocol */
#define PPP_ACSCP	0x8235	/* Apple Client Server Control Protocol */
#define PPP_LCP		0xc021	/* Link Control Protocol */
#define PPP_PAP		0xc023	/* Password Authentication Protocol */
#define PPP_LQR		0xc025	/* Link Quality Report protocol */
#define PPP_CHAP	0xc223	/* Cryptographic Handshake Auth. Protocol */
#define PPP_CBCP	0xc029	/* Callback Control Protocol */
#define PPP_EAP		0xc227	/* Extensible Authentication Protocol */

/*
 * Values for FCS calculations.
 */

#define PPP_INITFCS	0xffff	/* Initial FCS value */
#define PPP_GOODFCS	0xf0b8	/* Good final FCS value */
#define PPP_FCS(fcs, c)	(((fcs) >> 8) ^ fcstab[((fcs) ^ (c)) & 0xff])

/*
 * Extended asyncmap - allows any character to be escaped.
 */

typedef u_int32_t		ext_accm[8];

/*
 * What to do with network protocol (NP) packets.
 */
enum NPmode {
    NPMODE_PASS,		/* pass the packet through */
    NPMODE_DROP,		/* silently drop the packet */
    NPMODE_ERROR,		/* return an error */
    NPMODE_QUEUE		/* save it up for later. */
};

/*
 *  Filter address for network protocol (NP) packets.
 */
enum NPAFmode {
    NPAFMODE_SRC_IN = 1,	/* filter source address of incoming packets */
    NPAFMODE_SRC_OUT = 2,	/* filter source address of outgoing packets */
    NPAFMODE_DHCP_INTERCEPT_SERVER = 4,	/* intercept DHCP packets as server */
    NPAFMODE_DHCP_INTERCEPT_CLIENT = 8	/* intercept DHCP packets as client */
};

#if __DARWIN_ALIGN_POWER
#pragma options align=power
#endif

/*
 * Statistics for LQRP and pppstats
 */
struct pppstat	{
    u_int32_t	ppp_discards;	/* # frames discarded */

    u_int32_t	ppp_ibytes;	/* bytes received */
    u_int32_t	ppp_ioctects;	/* bytes received not in error */
    u_int32_t	ppp_ipackets;	/* packets received */
    u_int32_t	ppp_ierrors;	/* receive errors */
    u_int32_t	ppp_ilqrs;	/* # LQR frames received */

    u_int32_t	ppp_obytes;	/* raw bytes sent */
    u_int32_t	ppp_ooctects;	/* frame bytes sent */
    u_int32_t	ppp_opackets;	/* packets sent */
    u_int32_t	ppp_oerrors;	/* transmit errors */ 
    u_int32_t	ppp_olqrs;	/* # LQR frames sent */
};

struct vjstat {
    u_int32_t	vjs_packets;	/* outbound packets */
    u_int32_t	vjs_compressed;	/* outbound compressed packets */
    u_int32_t	vjs_searches;	/* searches for connection state */
    u_int32_t	vjs_misses;	/* times couldn't find conn. state */
    u_int32_t	vjs_uncompressedin; /* inbound uncompressed packets */
    u_int32_t	vjs_compressedin;   /* inbound compressed packets */
    u_int32_t	vjs_errorin;	/* inbound unknown type packets */
    u_int32_t	vjs_tossed;	/* inbound packets tossed because of error */
};

struct compstat {
    u_int32_t	unc_bytes;	/* total uncompressed bytes */
    u_int32_t	unc_packets;	/* total uncompressed packets */
    u_int32_t	comp_bytes;	/* compressed bytes */
    u_int32_t	comp_packets;	/* compressed packets */
    u_int32_t	inc_bytes;	/* incompressible bytes */
    u_int32_t	inc_packets;	/* incompressible packets */

    /* the compression ratio is defined as in_count / bytes_out */
    u_int32_t       in_count;	/* Bytes received */
    u_int32_t       bytes_out;	/* Bytes transmitted */

    double	ratio;		/* not computed in kernel. */
};

struct ppp_stats {
    struct pppstat	p;	/* basic PPP statistics */
    struct vjstat	vj;	/* VJ header compression statistics */
};

struct ppp_comp_stats {
    struct compstat	c;	/* packet compression statistics */
    struct compstat	d;	/* packet decompression statistics */
};

/*
 * The following structure records the time in seconds since
 * the last NP packet was sent or received.
 */
struct ppp_idle {
    u_int32_t xmit_idle;		/* time since last NP packet sent */
    u_int32_t recv_idle;		/* time since last NP packet received */
};

#if __DARWIN_ALIGN_POWER
#pragma options align=reset
#endif

#ifndef __P
#ifdef __STDC__
#define __P(x)	x
#else
#define __P(x)	()
#endif
#endif

#endif /* _PPP_DEFS_H_ */
