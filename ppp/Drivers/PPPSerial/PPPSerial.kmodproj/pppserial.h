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


#ifndef __PPPSERIAL_H__
#define __PPPSERIAL_H_


/*
 * Ioctl definitions.
 */

#define	PPPIOCGFLAGS	_IOR('t', 90, int)	/* get configuration flags */
#define	PPPIOCSFLAGS	_IOW('t', 89, int)	/* set configuration flags */
#define	PPPIOCGASYNCMAP	_IOR('t', 88, int)	/* get async map */
#define	PPPIOCSASYNCMAP	_IOW('t', 87, int)	/* set async map */
#define	PPPIOCGUNIT	_IOR('t', 86, int)	/* get ppp unit number */
#define	PPPIOCGRASYNCMAP _IOR('t', 85, int)	/* get receive async map */
#define	PPPIOCSRASYNCMAP _IOW('t', 84, int)	/* set receive async map */
#define	PPPIOCGMRU	_IOR('t', 83, int)	/* get max receive unit */
#define	PPPIOCSMRU	_IOW('t', 82, int)	/* set max receive unit */
#define	PPPIOCSMAXCID	_IOW('t', 81, int)	/* set VJ max slot ID */
#define PPPIOCGXASYNCMAP _IOR('t', 80, ext_accm) /* get extended ACCM */
#define PPPIOCSXASYNCMAP _IOW('t', 79, ext_accm) /* set extended ACCM */
#define PPPIOCXFERUNIT	_IO('t', 78)		/* transfer PPP unit */
#define PPPIOCSCOMPRESS	_IOW('t', 77, struct ppp_option_data)
#define PPPIOCGNPMODE	_IOWR('t', 76, struct npioctl) /* get NP mode */
#define PPPIOCSNPMODE	_IOW('t', 75, struct npioctl)  /* set NP mode */
#define PPPIOCGIDLE	_IOR('t', 74, struct ppp_idle) /* get idle time */

                           
/*
 * Bit definitions for flags.
 * these flags are specific to this interface
 * the common flags for ppp interfaces are somewhere else
 */
#define LK_LOG_INPKT	0x00020000	/* log contents of good pkts recvd */
#define LK_LOG_OUTPKT	0x00040000	/* log contents of pkts sent */
#define LK_LOG_RAWIN	0x00080000	/* log all chars received */
#define LK_LOG_FLUSH	0x00100000	/* log all chars flushed */
#define LK_RCV_B7_0	0x01000000	/* have rcvd char with bit 7 = 0 */
#define LK_RCV_B7_1	0x02000000	/* have rcvd char with bit 7 = 1 */
#define LK_RCV_EVNP	0x04000000	/* have rcvd char with even parity */
#define LK_RCV_ODDP	0x08000000	/* have rcvd char with odd parity */
#define	LK_MASK		0x0fff00ff	/* bits that user can change */

/*
 * Values for FCS calculations.
 */
#define PPP_FCSLEN	2	/* octets for FCS */
#define PPP_INITFCS	0xffff	/* Initial FCS value */
#define PPP_GOODFCS	0xf0b8	/* Good final FCS value */
#define PPP_FCS(fcs, c)	(((fcs) >> 8) ^ fcstab[((fcs) ^ (c)) & 0xff])


int pppserial_init();
int pppserial_dispose();


#endif