/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * natd - Network Address Translation Daemon for FreeBSD.
 *
 * This software is provided free of charge, with no 
 * warranty of any kind, either expressed or implied.
 * Use at your own risk.
 * 
 * You may copy, modify and distribute this software (icmp.c) freely.
 *
 * Ari Suutari <suutari@iki.fi>
 *
 * Based upon:
 * $FreeBSD: src/sbin/natd/icmp.c,v 1.6 1999/08/28 00:13:45 peter Exp $
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>

#include <netdb.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#include <alias.h>

#include "natd.h"

int SendNeedFragIcmp (int sock, struct ip* failedDgram, int mtu)
{
	char			icmpBuf[IP_MAXPACKET];
	struct ip*		ip;
	struct icmp*		icmp;
	int			icmpLen;
	int			failBytes;
	int			failHdrLen;
	struct sockaddr_in	addr;
	int			wrote;
	struct in_addr		swap;
/*
 * Don't send error if packet is
 * not the first fragment.
 */
	if (ntohs (failedDgram->ip_off) & ~(IP_MF | IP_DF))
		return 0;
/*
 * Dont respond if failed datagram is ICMP.
 */
	if (failedDgram->ip_p == IPPROTO_ICMP)
		return 0;
/*
 * Start building the message.
 */
	ip   = (struct ip*) icmpBuf;
	icmp = (struct icmp*) (icmpBuf + sizeof (struct ip));
/*
 * Complete ICMP part.
 */
	icmp->icmp_type  	= ICMP_UNREACH;
	icmp->icmp_code		= ICMP_UNREACH_NEEDFRAG;
	icmp->icmp_cksum	= 0;
	icmp->icmp_void		= 0;
	icmp->icmp_nextmtu	= htons (mtu);
/*
 * Copy header + 64 bits of original datagram.
 */
	failHdrLen = (failedDgram->ip_hl << 2);
	failBytes  = failedDgram->ip_len - failHdrLen;
	if (failBytes > 8)
		failBytes = 8;

	failBytes += failHdrLen;
	icmpLen    = ICMP_MINLEN + failBytes;

	memcpy (&icmp->icmp_ip, failedDgram, failBytes);
/*
 * Calculate checksum.
 */
	icmp->icmp_cksum = PacketAliasInternetChecksum ((u_short*) icmp,
							icmpLen);
/*
 * Add IP header using old IP header as template.
 */
	memcpy (ip, failedDgram, sizeof (struct ip));

	ip->ip_v	= 4;
	ip->ip_hl	= 5;
	ip->ip_len	= htons (sizeof (struct ip) + icmpLen);
	ip->ip_p	= IPPROTO_ICMP;
	ip->ip_tos	= 0;

	swap = ip->ip_dst;
	ip->ip_dst = ip->ip_src;
	ip->ip_src = swap;

	PacketAliasIn ((char*) ip, IP_MAXPACKET);

	addr.sin_family		= AF_INET;
	addr.sin_addr		= ip->ip_dst;
	addr.sin_port		= 0;
/*
 * Put packet into processing queue.
 */
	wrote = sendto (sock, 
		        icmp,
	    		icmpLen,
	    		0,
	    		(struct sockaddr*) &addr,
	    		sizeof addr);
	
	if (wrote != icmpLen)
		Warn ("Cannot send ICMP message.");

	return 1;
}


