
#import <stdio.h>
#import <stdlib.h>
#import <unistd.h>
#import <netinet/in.h>
#import <netinet/udp.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/bootp.h>
#import <arpa/inet.h>
#import <string.h>
#import "dhcplib.h"

void
dhcp_print_packet(struct dhcp *dp, int pkt_len)
{
    int i, j, len;
    
    if (pkt_len == 0)
	return;
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
    if (packet->bp_op != BOOTREPLY
	|| ntohl(packet->bp_xid) != xid
	|| (packet->bp_htype != hwtype)
	|| (packet->bp_hlen != hwlen)
	|| bcmp(packet->bp_chaddr, hwaddr, hwlen)) {
	return (FALSE);
    }
    return (TRUE);
}
