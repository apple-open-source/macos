#ifndef _S_UTIL_H
#define _S_UTIL_H
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <mach/boolean.h>
#include <string.h>
#include <ctype.h>
#include <net/ethernet.h>

#define IP_FORMAT	"%d.%d.%d.%d"
#define IP_CH(ip, i)	(((u_char *)(ip))[i])
#define IP_LIST(ip)	IP_CH(ip,0),IP_CH(ip,1),IP_CH(ip,2),IP_CH(ip,3)

#define EA_FORMAT	"%x:%x:%x:%x:%x:%x"
#define EA_CH(e, i)	((u_char)(((struct ether_addr *)(e))->ether_addr_octet[(i)]))
#define EA_LIST(ea)	EA_CH(ea,0),EA_CH(ea,1),EA_CH(ea,2),EA_CH(ea,3),EA_CH(ea,4),EA_CH(ea,5)

typedef struct {
	struct in_addr	start;
	struct in_addr	end;
} ip_range_t;

static __inline__ u_long 
iptohl(struct in_addr ip)
{
    return (ntohl(ip.s_addr));
}

static __inline__ struct in_addr 
hltoip(u_long l)
{
    struct in_addr ip;

    ip.s_addr = htonl(l);
    return (ip);
}

static __inline__ boolean_t
in_subnet(struct in_addr netaddr, struct in_addr netmask, struct in_addr ip)
{
    if ((iptohl(ip) & iptohl(netmask)) != iptohl(netaddr)) {
	return (FALSE);
    }
    return (TRUE);
}

int 	ipRangeCmp(ip_range_t * a_p, ip_range_t * b_p, boolean_t * overlap);

int	nbits_host(struct in_addr mask);

u_char *inet_nettoa(struct in_addr addr, struct in_addr mask);

long	random_range(long bottom, long top);

#define USECS_PER_SEC	1000000
void	timeval_subtract(struct timeval tv1, struct timeval tv2, 
			 struct timeval * result);

void	timeval_add(struct timeval tv1, struct timeval tv2,
		    struct timeval * result);

int	timeval_compare(struct timeval tv1, struct timeval tv2);

void	print_data(u_char * data_p, int n_bytes);

int	create_path(u_char * dirname, mode_t mode);

char *  tagtext_get(char * data, char * data_end, char * tag, char * * end_p);

void	timestamp_syslog(char * msg);
void	timestamp_printf(char * msg);
int	ether_cmp(struct ether_addr * e1, struct ether_addr * e2);
int	dns_hostname_is_clean(const char * source_str);

#endif _S_UTIL_H
