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
/* $Header: /cvs/Darwin/Commands/NeXT/network_cmds/tcpdump.tproj/ntp.h,v 1.1.1.1 1999/05/02 03:58:32 wsanchez Exp $ */

/*
 * Based on ntp.h from the U of MD implementation
 *	This file is based on Version 2 of the NTP spec (RFC1119).
 */

/*
 *  Definitions for the masses
 */
#define	JAN_1970	2208988800	/* 1970 - 1900 in seconds */

/*
 * Structure definitions for NTP fixed point values
 *
 *    0			  1		      2			  3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |			       Integer Part			     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |			       Fraction Part			     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *    0			  1		      2			  3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |		  Integer Part	     |	   Fraction Part	     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
struct l_fixedpt {
	u_int32_t int_part;
	u_int32_t fraction;
};

struct s_fixedpt {
	u_short int_part;
	u_short fraction;
};

/*  =================  Table 3.3. Packet Variables   ================= */
/*
 *    0			  1		      2			  3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |LI | VN  | Mode|	  Stratum    |	    Poll     |	 Precision   |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |			   Synchronizing Distance		     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |			  Synchronizing Dispersion		     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |			Reference Clock Identifier		     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |								     |
 *   |		       Reference Timestamp (64 bits)		     |
 *   |								     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |								     |
 *   |		       Originate Timestamp (64 bits)		     |
 *   |								     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |								     |
 *   |			Receive Timestamp (64 bits)		     |
 *   |								     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |								     |
 *   |			Transmit Timestamp (64 bits)		     |
 *   |								     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
struct ntpdata {
	u_char status;		/* status of local clock and leap info */
	u_char stratum;		/* Stratum level */
	u_char ppoll;		/* poll value */
	int precision:8;
	struct s_fixedpt distance;
	struct s_fixedpt dispersion;
	u_int32_t refid;
	struct l_fixedpt reftime;
	struct l_fixedpt org;
	struct l_fixedpt rec;
	struct l_fixedpt xmt;
};
/*
 *	Leap Second Codes (high order two bits)
 */
#define	NO_WARNING	0x00	/* no warning */
#define	PLUS_SEC	0x40	/* add a second (61 seconds) */
#define	MINUS_SEC	0x80	/* minus a second (59 seconds) */
#define	ALARM		0xc0	/* alarm condition (clock unsynchronized) */

/*
 *	Clock Status Bits that Encode Version
 */
#define	NTPVERSION_1	0x08
#define	VERSIONMASK	0x38
#define LEAPMASK	0xc0
#define	MODEMASK	0x07

/*
 *	Code values
 */
#define	MODE_UNSPEC	0	/* unspecified */
#define	MODE_SYM_ACT	1	/* symmetric active */
#define	MODE_SYM_PAS	2	/* symmetric passive */
#define	MODE_CLIENT	3	/* client */
#define	MODE_SERVER	4	/* server */
#define	MODE_BROADCAST	5	/* broadcast */
#define	MODE_RES1	6	/* reserved */
#define	MODE_RES2	7	/* reserved */

/*
 *	Stratum Definitions
 */
#define	UNSPECIFIED	0
#define	PRIM_REF	1	/* radio clock */
#define	INFO_QUERY	62	/* **** THIS implementation dependent **** */
#define	INFO_REPLY	63	/* **** THIS implementation dependent **** */
