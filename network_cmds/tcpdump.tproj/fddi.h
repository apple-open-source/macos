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
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @(#) $Header: /cvs/Darwin/Commands/NeXT/network_cmds/tcpdump.tproj/fddi.h,v 1.1.1.1 1999/05/02 03:58:31 wsanchez Exp $ (LBL)
 */

/*
 * Based on Ultrix if_fddi.h
 */

/*
 * This stuff should come from a system header file, but there's no
 * obviously portable way to do that and it's not really going
 * to change from system to system (except for the padding business).
 */

struct fddi_header {
	u_char  fddi_fc;		/* frame control */
	u_char  fddi_dhost[6];
	u_char  fddi_shost[6];
};


/* Useful values for fddi_fc (frame control) field */

/*
 * FDDI Frame Control bits
 */
#define	FDDIFC_C		0x80		/* Class bit */
#define	FDDIFC_L		0x40		/* Address length bit */
#define	FDDIFC_F		0x30		/* Frame format bits */
#define	FDDIFC_Z		0x0f		/* Control bits */

/*
 * FDDI Frame Control values. (48-bit addressing only).
 */
#define	FDDIFC_VOID		0x40		/* Void frame */
#define	FDDIFC_NRT		0x80		/* Nonrestricted token */
#define	FDDIFC_RT		0xc0		/* Restricted token */
#define	FDDIFC_SMT_INFO		0x41		/* SMT Info */
#define	FDDIFC_SMT_NSA		0x4F		/* SMT Next station adrs */
#define	FDDIFC_MAC_BEACON	0xc2		/* MAC Beacon frame */
#define	FDDIFC_MAC_CLAIM	0xc3		/* MAC Claim frame */
#define	FDDIFC_LLC_ASYNC	0x50		/* Async. LLC frame */
#define	FDDIFC_LLC_SYNC		0xd0		/* Sync. LLC frame */
#define	FDDIFC_IMP_ASYNC	0x60		/* Implementor Async. */
#define	FDDIFC_IMP_SYNC		0xe0		/* Implementor Synch. */
#define FDDIFC_SMT		0x40		/* SMT frame */
#define FDDIFC_MAC		0xc0		/* MAC frame */

#define	FDDIFC_CLFF		0xF0		/* Class/Length/Format bits */
#define	FDDIFC_ZZZZ		0x0F		/* Control bits */
