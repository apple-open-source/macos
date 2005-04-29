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

/*
 * Modification History
 *
 * March 9, 2004		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <stdlib.h>
#include <mach/mach.h>
#include <mach/mach_error.h>

#include "dnsinfo.h"
#include "dnsinfo_private.h"
#include "shared_dns_info.h"


static boolean_t
add_list(void **padding, uint32_t *n_padding, int32_t count, int32_t size, void **list)
{
	int32_t	need;

	need = count * size;
	if (need > *n_padding) {
		return FALSE;
	}

	*list = (need == 0) ? NULL : *padding;
	*padding   += need;
	*n_padding -= need;
	return TRUE;
}


static _dns_config_buf_t *
copy_dns_info()
{
	uint8_t			*buf	= NULL;
	dnsDataOut_t		dataRef	= NULL;
	mach_msg_type_number_t	dataLen	= 0;
	mach_port_t		server;
	kern_return_t		status;

	server = _dns_configuration_server_port();
	if (server == MACH_PORT_NULL) {
		return NULL;
	}

	status = shared_dns_infoGet(server, &dataRef, &dataLen);
	(void)mach_port_deallocate(mach_task_self(), server);
	if (status != KERN_SUCCESS) {
		mach_error("shared_dns_infoGet():", status);
		return NULL;
	}

	if (dataRef != NULL) {
		if (dataLen >= sizeof(_dns_config_buf_t)) {
			_dns_config_buf_t	*config		= (_dns_config_buf_t *)dataRef;
			uint32_t		len;
			uint32_t		n_padding       = ntohl(config->n_padding);

			len = dataLen + n_padding;
			buf = malloc(len);
			bcopy((void *)dataRef, buf, dataLen);
			bzero(&buf[dataLen], n_padding);
		}

		status = vm_deallocate(mach_task_self(), (vm_address_t)dataRef, dataLen);
		if (status != KERN_SUCCESS) {
			mach_error("vm_deallocate():", status);
			free(buf);
			return NULL;
		}
	}

	return (_dns_config_buf_t *)buf;
}


static dns_resolver_t *
expand_resolver(_dns_resolver_buf_t *buf, uint32_t n_buf, void **padding, uint32_t *n_padding)
{
	dns_attribute_t		*attribute;
	uint32_t		n_attribute;
	int32_t			n_nameserver    = 0;
	int32_t			n_search	= 0;
	int32_t			n_sortaddr      = 0;
	dns_resolver_t		*resolver	= (dns_resolver_t *)&buf->resolver;

	if (n_buf < sizeof(_dns_resolver_buf_t)) {
		goto error;
	}

	// initialize domain

	resolver->domain = NULL;

	// initialize nameserver list

	resolver->n_nameserver = ntohl(resolver->n_nameserver);
	if (!add_list(padding,
		      n_padding,
		      resolver->n_nameserver,
		      sizeof(struct sockaddr *),
		      (void **)&resolver->nameserver)) {
		goto error;
	}

	// initialize port

	resolver->port = ntohs(resolver->port);

	// initialize search list

	resolver->n_search = ntohl(resolver->n_search);
	if (!add_list(padding,
		      n_padding,
		      resolver->n_search,
		      sizeof(char *),
		      (void **)&resolver->search)) {
		goto error;
	}

	// initialize sortaddr list

	resolver->n_sortaddr = ntohl(resolver->n_sortaddr);
	if (!add_list(padding,
		      n_padding,
		      resolver->n_sortaddr,
		      sizeof(dns_sortaddr_t *),
		      (void **)&resolver->sortaddr)) {
		goto error;
	}

	// initialize options

	resolver->options = NULL;

	// initialize timeout

	resolver->timeout = ntohl(resolver->timeout);

	// initialize search_order

	resolver->search_order = ntohl(resolver->search_order);

	// process resolver buffer "attribute" data

	n_attribute = n_buf - sizeof(_dns_resolver_buf_t);
	attribute   = (dns_attribute_t *)&buf->attribute[0];
	if (n_attribute != ntohl(buf->n_attribute)) {
		goto error;
	}

	while (n_attribute >= sizeof(dns_attribute_t)) {
		int32_t	attribute_length	= ntohl(attribute->length);

		switch (ntohl(attribute->type)) {
			case RESOLVER_ATTRIBUTE_DOMAIN :
				resolver->domain = (char *)&attribute->attribute[0];
				break;

			case RESOLVER_ATTRIBUTE_ADDRESS :
				resolver->nameserver[n_nameserver++] = (struct sockaddr *)&attribute->attribute[0];
				break;

			case RESOLVER_ATTRIBUTE_SEARCH :
				resolver->search[n_search++] = (char *)&attribute->attribute[0];
				break;

			case RESOLVER_ATTRIBUTE_SORTADDR :
				resolver->sortaddr[n_sortaddr++] = (dns_sortaddr_t *)&attribute->attribute[0];
				break;

			case RESOLVER_ATTRIBUTE_OPTIONS :
				resolver->options = (char *)&attribute->attribute[0];
				break;

			default :
				break;
		}

		attribute   = (dns_attribute_t *)((void *)attribute + attribute_length);
		n_attribute -= attribute_length;
	}

	if ((n_nameserver != resolver->n_nameserver) ||
	    (n_search     != resolver->n_search    ) ||
	    (n_sortaddr   != resolver->n_sortaddr  )) {
		goto error;
	}

	return resolver;

    error :

	return NULL;
}


static dns_config_t *
expand_config(_dns_config_buf_t *buf)
{
	dns_attribute_t		*attribute;
	dns_config_t		*config		= (dns_config_t *)buf;
	uint32_t		n_attribute;
	uint32_t		n_padding;
	int32_t			n_resolver      = 0;
	void			*padding;

	// establish padding

	padding   = &buf->attribute[ntohl(buf->n_attribute)];
	n_padding = ntohl(buf->n_padding);

	// initialize resolver list

	config->n_resolver = ntohl(config->n_resolver);
	if (!add_list(&padding,
		      &n_padding,
		      config->n_resolver,
		      sizeof(dns_resolver_t *),
		      (void **)&config->resolver)) {
		goto error;
	}

	// process configuration buffer "attribute" data

	n_attribute = ntohl(buf->n_attribute);
	attribute   = (dns_attribute_t *)&buf->attribute[0];

	while (n_attribute >= sizeof(dns_attribute_t)) {
		int32_t	attribute_length	= ntohl(attribute->length);

		switch (ntohl(attribute->type)) {
			case CONFIG_ATTRIBUTE_RESOLVER : {
				dns_resolver_t	*resolver;

				// expand resolver buffer

				resolver = expand_resolver((_dns_resolver_buf_t *)&attribute->attribute[0],
							   attribute_length - sizeof(dns_attribute_t),
							   &padding,
							   &n_padding);
				if (resolver == NULL) {
					goto error;
				}

				// add resolver to config list

				config->resolver[n_resolver++] = resolver;

				break;
			}

			default :
				break;
		}

		attribute   = (dns_attribute_t *)((void *)attribute + attribute_length);
		n_attribute -= attribute_length;
	}

	if (n_resolver != config->n_resolver) {
		goto error;
	}

	return config;

    error :

	return NULL;
}


__private_extern__
const char *
dns_configuration_notify_key()
{
	return _dns_configuration_notify_key();
}


__private_extern__
dns_config_t *
dns_configuration_copy()
{
	_dns_config_buf_t	*buf;
	dns_config_t		*config;

	buf = copy_dns_info();
	if (buf == NULL) {
		return NULL;
	}

	config = expand_config(buf);
	if (config == NULL) {
		free(buf);
		return NULL;
	}

	return config;
}


__private_extern__
void
dns_configuration_free(dns_config_t *config)
{
	if (config == NULL) {
		return;
	}

	free((void *)config);
	return;
}
