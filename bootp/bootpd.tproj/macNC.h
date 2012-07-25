
#ifndef _S_MACNC_H
#define _S_MACNC_H

/*
 * Copyright (c) 1999-2008 Apple Inc. All rights reserved.
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
			       struct dhcp * reply, const char * hostname, 
			       struct in_addr servip, 
			       int host_number, dhcpoa_t * options,
			       uid_t uid, const char * afp_user, 
			       const char * passwd);

NBSPEntry *
macNC_allocate_shadow(const char * machine_name, int host_number, 
		      uid_t uid, gid_t gid, const char * shadow_name);

boolean_t	macNC_get_client_info(struct dhcp * pkt, int pkt_size, 
				      dhcpol_t * options, 
				      u_int * client_version);
void
macNC_unlink_shadow(int host_number, const char * hostname);

boolean_t
set_privs(const char * path, struct stat * sb_p, uid_t uid, gid_t gid,
	  mode_t mode, boolean_t unlock);


#endif /* _S_MACNC_H */
