/*
 * Copyright (c) 2000 Apple Inc. All rights reserved.
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
dhcp_fprint_packet(FILE * f, struct dhcp * dp, int pkt_len)
{
    int i, j, len;
    
    if (pkt_len < sizeof(struct dhcp)) {
	fprintf(f, "Packet is too short %d < %d\n", pkt_len,
		(int)sizeof(struct dhcp));
	return;
    }
    fprintf(f, "op = ");
    if (dp->dp_op == BOOTREQUEST) {
	fprintf(f, "BOOTREQUEST\n");
    }
    else if (dp->dp_op == BOOTREPLY) {
	fprintf(f, "BOOTREPLY\n");
    }
    else {
	i = dp->dp_op;
	fprintf(f, "OP(%d)\n", i);
    }
    
    i = dp->dp_htype;
    fprintf(f, "htype = %d\n", i);
    
    fprintf(f, "flags = %x\n", ntohs(dp->dp_flags));
    len = dp->dp_hlen;
    fprintf(f, "hlen = %d\n", len);
    
    i = dp->dp_hops;
    fprintf(f, "hops = %d\n", i);
    
    fprintf(f, "xid = %lu\n", (u_long)ntohl(dp->dp_xid));
    
    fprintf(f, "secs = %hu\n", ntohs(dp->dp_secs));
    
    fprintf(f, "ciaddr = %s\n", inet_ntoa(dp->dp_ciaddr));
    fprintf(f, "yiaddr = %s\n", inet_ntoa(dp->dp_yiaddr));
    fprintf(f, "siaddr = %s\n", inet_ntoa(dp->dp_siaddr));
    fprintf(f, "giaddr = %s\n", inet_ntoa(dp->dp_giaddr));
    
    fprintf(f, "chaddr = ");
    for (j = 0; j < len; j++) {
	i = dp->dp_chaddr[j];
	fprintf(f, "%0x", i);
	if (j < (len - 1)) fprintf(f, ":");
    }
    fprintf(f, "\n");
    
    fprintf(f, "sname = %s\n", dp->dp_sname);
    fprintf(f, "file = %s\n", dp->dp_file);
    
    {
	dhcpol_t t;
	
	dhcpol_init(&t);
	if (dhcpol_parse_packet(&t, dp, pkt_len, NULL)) {
	    fprintf(f, "options:\n");
	    dhcpol_fprint(f, &t);
	}
	dhcpol_free(&t);
    }
    fflush(f);
    return;
}

void
dhcp_print_packet(struct dhcp *dp, int pkt_len)
{
    dhcp_fprint_packet(stdout, dp, pkt_len);
    return;
}

boolean_t
dhcp_packet_match(struct bootp * packet, u_int32_t xid, 
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
