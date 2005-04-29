/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#ifndef __DNSINFO_H__
#define __DNSINFO_H__


/*
 * These routines provide access to the systems DNS configuration
 */


#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <sys/cdefs.h>


#define DEFAULT_SEARCH_ORDER    200000   /* search order for the "default" resolver domain name */


typedef struct {
	struct in_addr	address;
	struct in_addr	mask;
} dns_sortaddr_t;


typedef struct {
	char		*domain;	/* domain */
	int32_t		n_nameserver;	/* # nameserver */
	struct sockaddr	**nameserver;
	uint16_t	port;		/* port (in host byte order) */
	int32_t		n_search;	/* # search */
	char		**search;
	int32_t		n_sortaddr;	/* # sortaddr */
	dns_sortaddr_t	**sortaddr;
	char		*options;	/* options */
	uint32_t	timeout;	/* timeout */
	uint32_t	search_order;	/* search_order */
	void		*reserved[8];
} dns_resolver_t;


typedef struct {
	int32_t		n_resolver;	/* resolver configurations */
	dns_resolver_t	**resolver;
	void		*reserved[8];
} dns_config_t;


__BEGIN_DECLS

/*
 * DNS configuration access APIs
 */
const char *	dns_configuration_notify_key    ();
dns_config_t *	dns_configuration_copy		();
void		dns_configuration_free		(dns_config_t *config);

__END_DECLS

#endif __DNSINFO_H__
