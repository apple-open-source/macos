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

#import "dhcp_options.h"

/*
 * bootpd.h
 */
int
add_subnet_options(NIDomain_t * domain, u_char * hostname, 
		   struct in_addr iaddr, 
		   interface_t * intface, dhcpoa_t * options,
		   u_char * tags, int n);
boolean_t
bootp_add_bootfile(char * request_file, char * hostname, char * bootfile,
		   char * reply_file);
boolean_t	
get_dhcp_option(id subnet, int tag, void * buf, int * len_p);

void
host_parms_from_proplist(ni_proplist * pl_p, int index, struct in_addr * ip, 
			 u_char * * name, u_char * * bootfile);

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

#define NI_DHCP_OPTION_PREFIX	"dhcp_"
#include "globals.h"

typedef struct subnet_match_args {
    struct in_addr	giaddr;
    struct in_addr	ciaddr;
    interface_t *	if_p;
    boolean_t		has_binding;
} subnet_match_args_t;


typedef struct {
    ni_proplist		pl;
    ni_id		dir_id;
    ni_name		path;
    int			instance;
} PropList_t;

void
PropList_free(PropList_t * pl_p);

void 
PropList_init(PropList_t * pl_p, ni_name path);

boolean_t
PropList_read(PropList_t * pl_p);

ni_namelist *
PropList_lookup(PropList_t * pl_p, ni_name propname);

int
PropList_instance(PropList_t * pl_p);

