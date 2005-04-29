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

#ifndef __DNSINFO_CREATE_H__
#define __DNSINFO_CREATE_H__


/*
 * These routines provide access to the systems DNS configuration
 */


#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <sys/cdefs.h>


#include <dnsinfo.h>


typedef const struct __dns_create_config *      dns_create_config_t;
typedef const struct __dns_create_resolver *    dns_create_resolver_t;


__BEGIN_DECLS

/*
 * DNS configuration creation APIs
 */
dns_create_config_t     _dns_configuration_create       ();
void			_dns_configuration_add_resolver (dns_create_config_t *_config, dns_create_resolver_t _resolver);
_Bool			_dns_configuration_store	(dns_create_config_t *_config);
void			_dns_configuration_free		(dns_create_config_t *_config);

/*
 * DNS [resolver] configuration creation APIs
 */
dns_create_resolver_t   _dns_resolver_create();
void			_dns_resolver_set_domain	(dns_create_resolver_t *_resolver, const char *domain);
void			_dns_resolver_add_nameserver	(dns_create_resolver_t *_resolver, struct sockaddr *nameserver);
void			_dns_resolver_set_port		(dns_create_resolver_t *_resolver, uint32_t port);	// host byte order
void			_dns_resolver_add_search	(dns_create_resolver_t *_resolver, const char *search);
void			_dns_resolver_add_sortaddr	(dns_create_resolver_t *_resolver, dns_sortaddr_t *sortaddr);
void			_dns_resolver_set_options	(dns_create_resolver_t *_resolver, const char *options);
void			_dns_resolver_set_timeout	(dns_create_resolver_t *_resolver, uint32_t timeout);
void			_dns_resolver_set_order		(dns_create_resolver_t *_resolver, uint32_t order);
void			_dns_resolver_free		(dns_create_resolver_t *_resolver);

__END_DECLS

#endif __DNSINFO_CREATE_H__
