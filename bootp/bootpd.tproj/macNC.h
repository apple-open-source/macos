
#ifndef _S_MACNC_H
#define _S_MACNC_H

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

#include <mach/boolean.h>

/**
 ** Defines:
 **/

#include "afp.h"
#include "nbimages.h"

#define SHARED_DIR_PERMS	0775
#define SHARED_FILE_PERMS	0664

#define CLIENT_DIR_PERMS	0770
#define CLIENT_FILE_PERMS	0660

/**
 ** Types:
 **/
typedef int (*funcptr_t)(void * arg);

/*
 * MACNC_SERVER_VERSION
 * - the value we pass back to the client in the BOOTP reply
 */
#define MACNC_SERVER_VERSION		0

#define kNetBootShadowName	"Shadow"


/**
 ** Prototypes:
 **/
boolean_t	macNC_init();
boolean_t	macNC_allocate(NBImageEntryRef image_entry,
			       struct dhcp * reply, u_char * hostname, 
			       struct in_addr servip, 
			       int host_number, dhcpoa_t * options,
			       uid_t uid, u_char * afp_user, u_char * passwd);

NBSPEntry *
macNC_allocate_shadow(const char * machine_name, int host_number, 
		      uid_t uid, gid_t gid, const char * shadow_name);

boolean_t	macNC_get_client_info(struct dhcp * pkt, int pkt_size, 
				      dhcpol_t * options, 
				      u_int * client_version);
void
macNC_unlink_shadow(int host_number, u_char * hostname);

boolean_t
set_privs(u_char * path, struct stat * sb_p, uid_t uid, gid_t gid,
	  mode_t mode, boolean_t unlock);


#endif _S_MACNC_H
