/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* Cisco IGRP definitions */

/* IGRP Header */

struct igrphdr {
#ifdef WORDS_BIGENDIAN
	u_char ig_v:4;		/* protocol version number */
	u_char ig_op:4;		/* opcode */
#else
	u_char ig_op:4;		/* opcode */
	u_char ig_v:4;		/* protocol version number */
#endif
	u_char ig_ed;		/* edition number */
	u_short ig_as;		/* autonomous system number */
	u_short ig_ni;		/* number of subnet in local net */
	u_short ig_ns;		/* number of networks in AS */
	u_short ig_nx;		/* number of networks ouside AS */
	u_short ig_sum;		/* checksum of IGRP header & data */
};

#define IGRP_UPDATE	1
#define IGRP_REQUEST	2

/* IGRP routing entry */

struct igrprte {
	u_char igr_net[3];	/* 3 significant octets of IP address */
	u_char igr_dly[3];	/* delay in tens of microseconds */
	u_char igr_bw[3];	/* bandwidth in units of 1 kb/s */
	u_char igr_mtu[2];	/* MTU in octets */
	u_char igr_rel;		/* percent packets successfully tx/rx */
	u_char igr_ld;		/* percent of channel occupied */
	u_char igr_hct;		/* hop count */
};

#define IGRP_RTE_SIZE	14	/* don't believe sizeof ! */
