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
 * macNCOptions.h
 * - dhcp/bootp options specific to the macNC
 */

/*
 * Modification History:
 *
 * December 15, 1997	Dieter Siegmund (dieter@apple)
 * - created
 * November 19, 1999 	Dieter Siegmund (dieter@apple)
 * - converted to regular C
 */

#import "gen_dhcp_tags.h"
#import "gen_dhcp_types.h"
#import "dhcp_options.h"

typedef enum {
    /* macNC client request options */
    macNCtag_client_version_e 		= 220,
    macNCtag_client_info_e 		= 221,

    /* macNC server reply options */
    macNCtag_server_version_e 		= 230,
    macNCtag_server_info_e 		= 231,
    macNCtag_user_name_e		= 232,
    macNCtag_password_e			= 233,
    macNCtag_shared_system_file_e 	= 234,
    macNCtag_private_system_file_e 	= 235,
    macNCtag_page_file_e 		= 236,
    macNCtag_MacOS_machine_name_e	= 237,
    macNCtag_shared_system_shadow_file_e = 238,
    macNCtag_private_system_shadow_file_e = 239,
} macNCtag_t;

typedef enum {
    macNCtype_pstring_e		= dhcptype_last_e + 1,
    macNCtype_afp_path_e	= dhcptype_last_e + 2,
    macNCtype_afp_password_e	= dhcptype_last_e + 3,
} macNCtype_t;

#import "afp.h"

#define AFP_PATH_OVERHEAD	13
#define AFP_PATH_LIMIT		242
#define AFP_PATHTYPE_SHORT	1
#define AFP_PATHTYPE_LONG	2
#define AFP_PATH_SEPARATOR	'\0'

/*
 * MACNC_CLIENT_INFO
 */
#define MACNC_CLIENT_INFO		"Apple MacNC"

boolean_t
macNCopt_encodeAFPPath(struct in_addr iaddr, u_short port,
		       u_char * volname, unsigned long dirID,
		       u_char pathtype, u_char * pathname,
		       u_char separator, void * buf,
		       int * len_p, u_char * err);
boolean_t
macNCopt_str_to_type(unsigned char * str, 
		     int type, void * buf, int * len_p,
		     unsigned char * err);
