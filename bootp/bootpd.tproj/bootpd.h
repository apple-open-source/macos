
#ifndef _S_BOOTPD_H
#define _S_BOOTPD_H

/*
 * Copyright (c) 1999 Apple Inc. All rights reserved.
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

#include "dhcp_options.h"
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>
#include "netinfo.h"

typedef struct {
    interface_t *	if_p;
    struct dhcp *	pkt;
    int			pkt_length;
    dhcpol_t *		options_p;
    struct in_addr *	dstaddr_p;
    struct timeval *	time_in_p;
} request_t;

/*
 * bootpd.h
 */
int
add_subnet_options(char * hostname, 
		   struct in_addr iaddr, 
		   interface_t * intface, dhcpoa_t * options,
		   const uint8_t * tags, int n);
boolean_t
bootp_add_bootfile(const char * request_file, const char * hostname, 
		   const char * bootfile, char * reply_file,
		   int reply_file_size);
void
host_parms_from_proplist(ni_proplist * pl_p, int index, struct in_addr * ip, 
			 char * * name, char * * bootfile);

boolean_t
subnetAddressAndMask(struct in_addr giaddr, interface_t * intface,
		     struct in_addr * addr, struct in_addr * mask);
boolean_t
subnet_match(void * arg, struct in_addr iaddr);

boolean_t	
sendreply(interface_t * intf, struct bootp * bp, int n,
	  boolean_t broadcast, struct in_addr * dest_p);
boolean_t
ip_address_reachable(struct in_addr ip, struct in_addr giaddr, 
		     interface_t * intface);

void
set_number_from_plist(CFDictionaryRef plist, CFStringRef prop_name_cf,
		      const char * prop_name, uint32_t * val_p);

#define NI_DHCP_OPTION_PREFIX	"dhcp_"
#include "globals.h"

typedef struct subnet_match_args {
    struct in_addr	giaddr;
    struct in_addr	ciaddr;
    interface_t *	if_p;
    boolean_t		has_binding;
} subnet_match_args_t;

extern void
my_log(int priority, const char *message, ...);

boolean_t
detect_other_dhcp_server(interface_t * if_p);

void
disable_dhcp_on_interface(interface_t * if_p);

#endif /* _S_BOOTPD_H */
