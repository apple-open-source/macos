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


#ifndef __RADIUS_H__
#define __RADIUS_H__

#define MD4_SIGNATURE_SIZE	16	/* 16 bytes in a MD4 message digest */
#define MPPE_MAX_KEY_LEN        16      /* largest key length (128-bit) */


enum {
	RADIUS_USE_PAP		= 0x1,
	RADIUS_USE_CHAP		= 0x2,	// not supported
	RADIUS_USE_MSCHAP2	= 0x4,
	RADIUS_USE_EAP		= 0x8
};

struct auth_server {
	char	*address;
	char	*secret;
	int 	port;
	int 	timeout;
	int 	retries;
	u_int16_t 	proto;			// PAP/CHAP/MSCHAP2/EAP
};

// list of servers
extern struct auth_server **auth_servers;// array of authentication servers
extern int nb_auth_servers;				// number of authentication servers

// radius attributes
extern char		*nas_identifier;		// NAS Identifier to include in Radius packets
extern char		*nas_ip_address;		// NAS IP address to include in Radius packets
extern int		nas_port_type;			// default is virtual
extern int		tunnel_type;			// not specified

int radius_decryptmppekey(char *key, u_int8_t *attr_value, size_t attr_len, char *secret, char *authenticator);

int radius_eap_install();

#endif