/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
/*
 * dhcp.h
 * - definitions for DHCP (as specified in RFC2132)
 */
#import <sys/types.h>
#import <netinet/in.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/udp.h>

struct dhcp {
    u_char		dp_op;		/* packet opcode type */
    u_char		dp_htype;	/* hardware addr type */
    u_char		dp_hlen;	/* hardware addr length */
    u_char		dp_hops;	/* gateway hops */
    u_int32_t		dp_xid;		/* transaction ID */
    u_int16_t		dp_secs;	/* seconds since boot began */	
    u_int16_t		dp_flags;	/* flags */
    struct in_addr	dp_ciaddr;	/* client IP address */
    struct in_addr	dp_yiaddr;	/* 'your' IP address */
    struct in_addr	dp_siaddr;	/* server IP address */
    struct in_addr	dp_giaddr;	/* gateway IP address */
    u_char		dp_chaddr[16];	/* client hardware address */
    u_char		dp_sname[64];	/* server host name */
    u_char		dp_file[128];	/* boot file name */
    u_char		dp_options[0];	/* variable-length options field */
};

struct dhcp_packet {
    struct ip 		ip;
    struct udphdr 	udp;
    struct dhcp 	dhcp;
};

#define DHCP_MIN_OPTIONS_SIZE	312

/* dhcp message types */
#define DHCPDISCOVER	1
#define DHCPOFFER	2
#define DHCPREQUEST	3
#define DHCPDECLINE	4
#define DHCPACK		5
#define DHCPNAK		6
#define DHCPRELEASE	7
#define DHCPINFORM	8

typedef enum {
    dhcp_msgtype_none_e		= 0,
    dhcp_msgtype_discover_e 	= DHCPDISCOVER,
    dhcp_msgtype_offer_e	= DHCPOFFER,
    dhcp_msgtype_request_e	= DHCPREQUEST,
    dhcp_msgtype_decline_e	= DHCPDECLINE,
    dhcp_msgtype_ack_e		= DHCPACK,
    dhcp_msgtype_nak_e		= DHCPNAK,
    dhcp_msgtype_release_e	= DHCPRELEASE,
    dhcp_msgtype_inform_e	= DHCPINFORM,
} dhcp_msgtype_t;

static __inline__ unsigned char *
dhcp_msgtype_names(dhcp_msgtype_t type)
{
    unsigned char * names[] = {
	"<none>",
	"DISCOVER",
	"OFFER",
	"REQUEST",
	"DECLINE",
	"ACK",
	"NAK",
	"RELEASE",
	"INFORM",
    };
    if (type >= dhcp_msgtype_none_e && type <= dhcp_msgtype_inform_e)
	return (names[type]);
    return ("<unknown>");
}

/* overloaded option values */
#define DHCP_OVERLOAD_FILE	1
#define DHCP_OVERLOAD_SNAME	2
#define DHCP_OVERLOAD_BOTH	3

typedef int32_t			dhcp_time_secs_t; /* absolute time */
typedef int32_t			dhcp_lease_t;     /* relative time */
#define dhcp_time_hton		htonl
#define dhcp_time_ntoh		ntohl
#define dhcp_lease_hton		htonl
#define dhcp_lease_ntoh		ntohl	

#define DHCP_INFINITE_LEASE	((dhcp_lease_t)-1)
#define DHCP_INFINITE_TIME	((dhcp_time_secs_t)-1)

#define DHCP_FLAGS_BROADCAST	((u_short)0x0001)

typedef enum {
    dhcp_cstate_none_e 	= 0,
    dhcp_cstate_decline_e,
    dhcp_cstate_unbound_e,
    dhcp_cstate_init_e,
    dhcp_cstate_select_e,
    dhcp_cstate_bound_e,
    dhcp_cstate_init_reboot_e,
    dhcp_cstate_renew_e,
    dhcp_cstate_rebind_e,
    dhcp_cstate_last_e		= dhcp_cstate_rebind_e,
} dhcp_cstate_t;

static __inline__ const u_char *
dhcp_cstate_str(dhcp_cstate_t state)
{
    static const char * list[] = {"<none>", 
				  "DECLINE",
				  "UNBOUND",
				  "INIT", 
				  "SELECT", 
				  "BOUND",
				  "INIT/REBOOT", 
				  "RENEW", 
				  "REBIND"};
    if (state <= dhcp_cstate_last_e)
	return list[state];
    return ("<undefined>");
}
