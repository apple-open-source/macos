/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/bootp.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <string.h>
#include "dhcplib.h"

void
dhcp_print_packet(struct dhcp *dp, int pkt_len)
{
    int i, j, len;
    
    if (pkt_len < sizeof(struct dhcp)) {
	printf("Packet is too short %d < %d\n", pkt_len,
	       (int)sizeof(struct dhcp));
	return;
    }
    printf("op = ");
    if (dp->dp_op == BOOTREQUEST) printf("BOOTREQUEST\n");
    else if (dp->dp_op == BOOTREPLY) printf("BOOTREPLY\n");
    else
	{
	    i = dp->dp_op;
	    printf("%d\n", i);
	}
    
    i = dp->dp_htype;
    printf("htype = %d\n", i);
    
    printf("dp_flags = %x\n", dp->dp_flags);
    len = dp->dp_hlen;
    printf("hlen = %d\n", len);
    
    i = dp->dp_hops;
    printf("hops = %d\n", i);
    
    printf("xid = %lu\n", (u_long)ntohl(dp->dp_xid));
    
    printf("secs = %hu\n", dp->dp_secs);
    
    printf("ciaddr = %s\n", inet_ntoa(dp->dp_ciaddr));
    printf("yiaddr = %s\n", inet_ntoa(dp->dp_yiaddr));
    printf("siaddr = %s\n", inet_ntoa(dp->dp_siaddr));
    printf("giaddr = %s\n", inet_ntoa(dp->dp_giaddr));
    
    printf("chaddr = ");
    for (j = 0; j < len; j++)
	{
	    i = dp->dp_chaddr[j];
	    printf("%0x", i);
	    if (j < (len - 1)) printf(":");
	}
    printf("\n");
    
    printf("sname = %s\n", dp->dp_sname);
    printf("file = %s\n", dp->dp_file);
    
    {
	dhcpol_t t;
	
	dhcpol_init(&t);
	if (dhcpol_parse_packet(&t, dp, pkt_len, NULL)) {
	    printf("options:\n");
	    dhcpol_print(&t);
	}
	dhcpol_free(&t);
    }
}

boolean_t
dhcp_packet_match(struct bootp * packet, unsigned long xid, 
		  u_char hwtype, void * hwaddr, int hwlen)
{
    int		check_len;

    switch (hwtype) {
    default:
    case ARPHRD_ETHER:
	check_len = hwlen;
	break;
    case ARPHRD_IEEE1394:
	check_len = 0;
	break;
    }
    if (packet->bp_op != BOOTREPLY
	|| ntohl(packet->bp_xid) != xid
	|| (packet->bp_htype != hwtype)
	|| (packet->bp_hlen != check_len)
	|| (check_len != 0 && bcmp(packet->bp_chaddr, hwaddr, check_len))) {
	return (FALSE);
    }
    return (TRUE);
}
