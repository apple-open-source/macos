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
 * macNC.h
 * - definitions for Rhapsody Mac NC Boot Server
 */

/*
 * Modification History:
 *
 * December 2, 1997	Dieter Siegmund (dieter@apple)
 * - created
 */

#import <mach/boolean.h>

/**
 ** Defines:
 **/

#include "afp.h"

#define CFGPROP_SHADOW_SIZE_MEG		"shadow_size_meg"
#define CFGPROP_AFP_USERS_MAX		"afp_users_max"
#define CFGPROP_AGE_TIME_SECONDS	"age_time_seconds"

/*
 * MACNC_SERVER_VERSION
 * - the value we pass back to the client in the BOOTP reply
 */
#define MACNC_SERVER_VERSION		0

/*
 * MACNC_SERVER_CREATOR
 * - the _creator value in the host entry
 */
#define MACNC_SERVER_CREATOR		"bootpd"

/*
 * MACNC_CLIENT_TYPE
 * - the value stored in the "client_types" property in a subnet description 
 *   for the NC (see subnetDescr.[hm]) in bootplib
 */
#define MACNC_CLIENT_TYPE	"macNC"

/**
 ** Types:
 **/
typedef int (*funcptr_t)(void * arg);

/**
 ** Prototypes:
 **/
boolean_t	macNC_init();
boolean_t	macNC_allocate(struct dhcp * reply, u_char * hostname, 
			       struct in_addr servip, 
			       int host_number, dhcpoa_t * options,
			       uid_t uid, u_char * afp_user, u_char * passwd);

boolean_t	macNC_get_client_info(struct dhcp * pkt, int pkt_size, 
				      dhcpol_t * options, 
				      u_int * client_version);
void
macNC_unlink_shadow(int host_number, u_char * hostname);

/**
 ** Globals
 **/
extern gid_t		netboot_gid;
extern int		afp_users_max;
extern u_int32_t	age_time_seconds;
