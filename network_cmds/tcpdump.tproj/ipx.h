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
/*
 * IPX protocol formats 
 *
 * @(#) $Header: /cvs/Darwin/Commands/NeXT/network_cmds/tcpdump.tproj/ipx.h,v 1.1.1.1 1999/05/02 03:58:32 wsanchez Exp $
 */

/* well-known sockets */
#define	IPX_SKT_NCP		0x0451
#define	IPX_SKT_SAP		0x0452
#define	IPX_SKT_RIP		0x0453
#define	IPX_SKT_NETBIOS		0x0455
#define	IPX_SKT_DIAGNOSTICS	0x0456

/* IPX transport header */
struct ipxHdr {
    u_short	cksum;		/* Checksum */
    u_short	length;		/* Length, in bytes, including header */
    u_char	tCtl;		/* Transport Control (i.e. hop count) */
    u_char	pType;		/* Packet Type (i.e. level 2 protocol) */
    u_short	dstNet[2];	/* destination net */
    u_char	dstNode[6];	/* destination node */
    u_short	dstSkt;		/* destination socket */
    u_short	srcNet[2];	/* source net */
    u_char	srcNode[6];	/* source node */
    u_short	srcSkt;		/* source socket */
} ipx_hdr_t;

#define ipxSize	30

