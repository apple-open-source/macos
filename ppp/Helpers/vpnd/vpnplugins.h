/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#ifndef __VPNPLUGINS_H__
#define __VPNPLUGINS_H__

#include "vpnoptions.h"

/*
 * This struct contains pointers to a set of procedures for
 * doing operations on a "vpn_channel".  A vpn_channel provides
 * functionality for vpnd to listen for and accept connections
 * for a paticular VPN protocol.  After a connection is accepted
 * the vpn plugin will fork and exec a copy of pppd to handle the
 * connection.
 *
 *		return values for refuse:	
 *			-1 			error 
 *			socket# 	launch pppd and notify caller that server is full
 *			0			handled - do not launch pppd
 */

struct vpn_channel {
    /* read and allocate args to pass to pppd */
    int (*get_pppd_args) __P((struct vpn_params*, int));
    /* intialize the vpn plugin */
    int (*listen) __P((void));
    /* accept an incoming connection */
    int (*accept) __P((void));
    /* refuse an incoming connection */
    int (*refuse) __P((void));
    /* we're finished with the channel */
    void (*close) __P((void));
    /* health check function */
    int (*health_check) __P((int *, int));
    /* load balance redirect function */
    int (*lb_redirect) __P((struct in_addr *, struct in_addr *));
};
   
void init_address_lists(void);
int add_address(char* ip_address);
int add_address_range(char* ip_addr_start, char* ip_addr_end);
void begin_address_update(void);
void cancel_address_update(void);
void apply_address_update(void);
int address_avail(void);
int init_plugin(struct vpn_params *params);
int get_plugin_args(struct vpn_params* params, int reload);
void accept_connections(struct vpn_params* params);

#endif
