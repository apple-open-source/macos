/*
 * Copyright (c) 2004, 2006, 2009 Apple Inc. All rights reserved.
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

#include <stdlib.h>
#include <strings.h>
#include <mach/mach.h>
#include <mach/mach_error.h>

#include "dnsinfo_create.h"
#include "dnsinfo_private.h"
#include "shared_dns_info.h"


#define ROUNDUP(a, size) \
	(((a) & ((size)-1)) ? (1 + ((a) | ((size)-1))) : (a))


/*
 * to avoid extra calls to realloc() we want to pre-allocate the initial
 * resolver and configuration buffers of a sufficient size that they would
 * not normally need to be expanded.
 */
#define INITIAL_CONFIGURATION_BUF_SIZE  8192
#define INITIAL_RESOLVER_BUF_SIZE       1024


/*
 * DNS [configuration] buffer functions
 */


dns_create_config_t
_dns_configuration_create()
{
	_dns_config_buf_t	*config;

	config = calloc(1, INITIAL_CONFIGURATION_BUF_SIZE);
//	config->n_attribute = 0;
//	config->n_padding = 0;
	return (dns_create_config_t)config;
}


static void
config_add_attribute(dns_create_config_t	*_config,
		     uint32_t			attribute_type,
		     uint32_t			attribute_length,
		     void			*attribute,
		     uint32_t			extra_padding)
{
	_dns_config_buf_t	*config	= (_dns_config_buf_t *)*_config;
	dns_attribute_t		*header;
	int			i;
	uint32_t		newLen;
	uint32_t		newSize;
	uint32_t		oldLen;
	uint32_t		rounded_length;

	// add space

	oldLen         = ntohl(config->n_attribute);
	rounded_length = ROUNDUP(attribute_length, sizeof(uint32_t));
	newLen         = sizeof(dns_attribute_t) + rounded_length;
	newSize = sizeof(_dns_config_buf_t) + oldLen + newLen;
	if (newSize > INITIAL_CONFIGURATION_BUF_SIZE) {
		config = realloc(config, newSize);
	}
	config->n_attribute = htonl(ntohl(config->n_attribute) + newLen);

	// increment additional padding that will be needed (later)
	config->n_padding = htonl(ntohl(config->n_padding) + extra_padding);

	// add attribute [header]

	header = (dns_attribute_t *)&config->attribute[oldLen];
	header->type   = htonl(attribute_type);
	header->length = htonl(newLen);

	// add attribute [data]

	bcopy(attribute, &header->attribute[0], attribute_length);
	for (i = attribute_length; i < rounded_length; i++) {
		header->attribute[i] = 0;
	}

	*_config = (dns_create_config_t)config;
	return;
}


void
_dns_configuration_add_resolver(dns_create_config_t     *_config,
				dns_create_resolver_t	_resolver)
{
	_dns_config_buf_t	*config		= (_dns_config_buf_t *)*_config;
	uint32_t		padding		= 0;
	_dns_resolver_buf_t	*resolver	= (_dns_resolver_buf_t *)_resolver;

	/*
	 * compute the amount of space that will be needed for
	 * pointers to the resolver, the nameservers, the search
	 * list, and the sortaddr list.
	 */
	padding += sizeof(DNS_PTR(dns_resolver_t *, x));
	if (resolver->resolver.n_nameserver != 0) {
		padding += ntohl(resolver->resolver.n_nameserver) * sizeof(DNS_PTR(struct sockaddr *, x));
	}
	if (resolver->resolver.n_search != 0) {
		padding += ntohl(resolver->resolver.n_search) * sizeof(DNS_PTR(char *, x));
	}
	if (resolver->resolver.n_sortaddr != 0) {
		padding += ntohl(resolver->resolver.n_sortaddr) * sizeof(DNS_PTR(dns_sortaddr_t *, x));
	}

	if ((ntohl(resolver->resolver.flags) & DNS_RESOLVER_FLAGS_SCOPED) == 0) {
		config->config.n_resolver = htonl(ntohl(config->config.n_resolver) + 1);
		config_add_attribute(_config,
				     CONFIG_ATTRIBUTE_RESOLVER,
				     sizeof(_dns_resolver_buf_t) + ntohl(resolver->n_attribute),
				     (void *)resolver,
				     padding);
	} else {
		config->config.n_scoped_resolver = htonl(ntohl(config->config.n_scoped_resolver) + 1);
		config_add_attribute(_config,
				     CONFIG_ATTRIBUTE_SCOPED_RESOLVER,
				     sizeof(_dns_resolver_buf_t) + ntohl(resolver->n_attribute),
				     (void *)resolver,
				     padding);
	}

	return;
}


_Bool
_dns_configuration_store(dns_create_config_t *_config)
{
	dnsDataOut_t		dataRef	= NULL;
	mach_msg_type_number_t	dataLen	= 0;
	mach_port_t		server;
	kern_return_t		status;

	server = _dns_configuration_server_port();
	if (server == MACH_PORT_NULL) {
		return FALSE;
	}

	if (_config != NULL) {
		_dns_config_buf_t	*config	= (_dns_config_buf_t *)*_config;

		if (config != NULL) {
			dataRef = (dnsDataOut_t)config;
			dataLen = sizeof(_dns_config_buf_t) + ntohl(config->n_attribute);
		}
	}

	status = shared_dns_infoSet(server, dataRef, dataLen);
	(void) mach_port_deallocate(mach_task_self(), server);
	if (status != KERN_SUCCESS) {
		mach_error("shared_dns_infoSet():", status);
		return FALSE;
	}

	return TRUE;
}


void
_dns_configuration_free(dns_create_config_t *_config)
{
	_dns_config_buf_t	*config	= (_dns_config_buf_t *)*_config;

	free(config);
	*_config = NULL;
	return;
}


/*
 * DNS resolver configuration functions
 */

dns_create_resolver_t
_dns_resolver_create()
{
	_dns_resolver_buf_t	*buf;

	buf = calloc(1, INITIAL_RESOLVER_BUF_SIZE);
//	buf->n_attribute = 0;
	return (dns_create_resolver_t)buf;
}


static void
_dns_resolver_add_attribute(dns_create_resolver_t	*_resolver,
			    uint32_t			attribute_type,
			    uint32_t			attribute_length,
			    void			*attribute)
{
	dns_attribute_t		*header;
	int			i;
	uint32_t		newLen;
	uint32_t		newSize;
	uint32_t		oldLen;
	_dns_resolver_buf_t	*resolver	= (_dns_resolver_buf_t *)*_resolver;
	uint32_t		rounded_length;

	// add space

	oldLen         = ntohl(resolver->n_attribute);
	rounded_length = ROUNDUP(attribute_length, sizeof(uint32_t));
	newLen         = sizeof(dns_attribute_t) + rounded_length;
	newSize = sizeof(_dns_resolver_buf_t) + oldLen + newLen;
	if (newSize > INITIAL_RESOLVER_BUF_SIZE) {
		resolver = realloc(resolver, newSize);
	}
	resolver->n_attribute = htonl(ntohl(resolver->n_attribute) + newLen);

	// add attribute [header]

	header = (dns_attribute_t *)&resolver->attribute[oldLen];
	header->type   = htonl(attribute_type);
	header->length = htonl(newLen);

	// add attribute [data]

	bcopy(attribute, &header->attribute[0], attribute_length);
	for (i = attribute_length; i < rounded_length; i++) {
		header->attribute[i] = 0;
	}

	*_resolver = (dns_create_resolver_t)resolver;
	return;
}


void
_dns_resolver_add_nameserver(dns_create_resolver_t *_resolver, struct sockaddr *nameserver)
{
	_dns_resolver_buf_t	*resolver	= (_dns_resolver_buf_t *)*_resolver;

	resolver->resolver.n_nameserver = htonl(ntohl(resolver->resolver.n_nameserver) + 1);
	_dns_resolver_add_attribute(_resolver, RESOLVER_ATTRIBUTE_ADDRESS, nameserver->sa_len, (void *)nameserver);
	return;
}


void
_dns_resolver_add_search(dns_create_resolver_t *_resolver, const char *search)
{
	_dns_resolver_buf_t	*resolver	= (_dns_resolver_buf_t *)*_resolver;

	resolver->resolver.n_search = htonl(ntohl(resolver->resolver.n_search) + 1);
	_dns_resolver_add_attribute(_resolver, RESOLVER_ATTRIBUTE_SEARCH, strlen(search) + 1, (void *)search);
	return;
}


void
_dns_resolver_set_domain(dns_create_resolver_t *_resolver, const char *domain)
{
	_dns_resolver_add_attribute(_resolver, RESOLVER_ATTRIBUTE_DOMAIN, strlen(domain) + 1, (void *)domain);
	return;
}


void
_dns_resolver_set_flags(dns_create_resolver_t *_resolver, uint32_t flags)
{
	_dns_resolver_buf_t	*resolver	= (_dns_resolver_buf_t *)*_resolver;

	resolver->resolver.flags = htonl(flags);
	return;
}


void
_dns_resolver_set_if_index(dns_create_resolver_t *_resolver, uint32_t if_index)
{
	_dns_resolver_buf_t	*resolver	= (_dns_resolver_buf_t *)*_resolver;

	resolver->resolver.if_index = htonl(if_index);
	return;
}


void
_dns_resolver_set_options(dns_create_resolver_t *_resolver, const char *options)
{
	_dns_resolver_add_attribute(_resolver, RESOLVER_ATTRIBUTE_OPTIONS, strlen(options) + 1, (void *)options);
	return;
}


void
_dns_resolver_set_order(dns_create_resolver_t *_resolver, uint32_t order)
{
	_dns_resolver_buf_t	*resolver	= (_dns_resolver_buf_t *)*_resolver;

	resolver->resolver.search_order = htonl(order);
	return;
}


void
_dns_resolver_set_port(dns_create_resolver_t *_resolver, uint16_t port)
{
	_dns_resolver_buf_t	*resolver	= (_dns_resolver_buf_t *)*_resolver;

	resolver->resolver.port = htons(port);
	return;
}


void
_dns_resolver_add_sortaddr(dns_create_resolver_t *_resolver, dns_sortaddr_t *sortaddr)
{
	_dns_resolver_buf_t	*resolver	= (_dns_resolver_buf_t *)*_resolver;

	resolver->resolver.n_sortaddr = htonl(ntohl(resolver->resolver.n_sortaddr) + 1);
	_dns_resolver_add_attribute(_resolver, RESOLVER_ATTRIBUTE_SORTADDR, sizeof(dns_sortaddr_t), (void *)sortaddr);
	return;
}


void
_dns_resolver_set_timeout(dns_create_resolver_t *_resolver, uint32_t timeout)
{
	_dns_resolver_buf_t	*resolver	= (_dns_resolver_buf_t *)*_resolver;

	resolver->resolver.timeout = htonl(timeout);
	return;
}


void
_dns_resolver_free(dns_create_resolver_t *_resolver)
{
	_dns_resolver_buf_t	*resolver	= (_dns_resolver_buf_t *)*_resolver;

	free(resolver);
	*_resolver = NULL;
	return;
}
