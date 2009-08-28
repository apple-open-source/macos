/*
 * Copyright (c) 2004, 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <mach/mach.h>
#include <mach/mach_error.h>

#include "dnsinfo.h"
#include "dnsinfo_private.h"
#include "shared_dns_info.h"


static pthread_once_t	_dns_initialized	= PTHREAD_ONCE_INIT;
static pthread_mutex_t	_dns_lock		= PTHREAD_MUTEX_INITIALIZER;
static mach_port_t	_dns_server		= MACH_PORT_NULL;


static void
__dns_fork_handler()
{
	// the process has forked (and we are the child process)
	_dns_server = MACH_PORT_NULL;
	return;
}


static void
__dns_initialize(void)
{
	// add handler to cleanup after fork()
	(void) pthread_atfork(NULL, NULL, __dns_fork_handler);

	return;
}


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

	// initialize runtime
	pthread_once(&_dns_initialized, __dns_initialize);

	// open a new session with the DNS configuration server
	server = _dns_server;
	while (TRUE) {
		if (server != MACH_PORT_NULL) {
			status = shared_dns_infoGet(server, &dataRef, &dataLen);
			if (status == KERN_SUCCESS) {
				break;
			}

			// our [cached] server port is not valid
			if (status != MACH_SEND_INVALID_DEST) {
				// if we got an unexpected error, don't retry
				fprintf(stderr,
					"dns_configuration_copy shared_dns_infoGet(): %s\n",
					mach_error_string(status));
				break;
			}
		}

		pthread_mutex_lock(&_dns_lock);
		if (_dns_server != MACH_PORT_NULL) {
			if (server == _dns_server) {
				// if the server we tried returned the error
				(void)mach_port_deallocate(mach_task_self(), server);
				_dns_server = _dns_configuration_server_port();
			} else {
				// another thread has refreshed the DNS server port
			}
		} else {
			_dns_server = _dns_configuration_server_port();
		}
		server = _dns_server;
		pthread_mutex_unlock(&_dns_lock);

		if (server == MACH_PORT_NULL) {
			// if server not available
			break;
		}
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
		      sizeof(DNS_PTR(struct sockaddr *, x)),
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
		      sizeof(DNS_PTR(char *, x)),
		      (void **)&resolver->search)) {
		goto error;
	}

	// initialize sortaddr list

	resolver->n_sortaddr = ntohl(resolver->n_sortaddr);
	if (!add_list(padding,
		      n_padding,
		      resolver->n_sortaddr,
		      sizeof(DNS_PTR(dns_sortaddr_t *, x)),
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
		      sizeof(DNS_PTR(dns_resolver_t *, x)),
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


const char *
dns_configuration_notify_key()
{
	return _dns_configuration_notify_key();
}


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


void
dns_configuration_free(dns_config_t *config)
{
	if (config == NULL) {
		return;
	}

	free((void *)config);
	return;
}
