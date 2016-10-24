/*
 * Copyright (c) 2013, 2015, 2016 Apple Inc. All rights reserved.
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

#ifndef _S_DNSINFO_INTERNAL_H
#define _S_DNSINFO_INTERNAL_H

#include <Availability.h>
#include <TargetConditionals.h>
#include <sys/cdefs.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCNetworkReachabilityInternal.h"
#include <arpa/inet.h>

#include <dnsinfo.h>
#include "dnsinfo_private.h"


__BEGIN_DECLS


#ifndef	my_log
#define	MY_LOG_DEFINED_LOCALLY
#define	my_log(__level, __format, ...)	SC_log(__level, __format, ## __VA_ARGS__)
#endif	// !my_log


#define	DNS_CONFIG_BUF_MAX	1024*1024


/*
 * claim space for a list [of pointers] from the expanded DNS configuration padding
 */
static __inline__ boolean_t
__dns_configuration_expand_add_list(void **padding, uint32_t *n_padding, int32_t count, int32_t size, void **list)
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


/*
 * expand a DNS "resolver" from the provided buffer
 */
static __inline__ dns_resolver_t *
_dns_configuration_expand_resolver(_dns_resolver_buf_t *buf, uint32_t n_buf, void **padding, uint32_t *n_padding)
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
	if (!__dns_configuration_expand_add_list(padding,
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
	if (!__dns_configuration_expand_add_list(padding,
						 n_padding,
						 resolver->n_search,
						 sizeof(DNS_PTR(char *, x)),
						 (void **)&resolver->search)) {
		goto error;
	}

	// initialize sortaddr list

	resolver->n_sortaddr = ntohl(resolver->n_sortaddr);
	if (!__dns_configuration_expand_add_list(padding,
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

	// initialize if_index

	resolver->if_index = ntohl(resolver->if_index);

	// initialize service_identifier

	resolver->service_identifier = ntohl(resolver->service_identifier);

	// initialize flags

	resolver->flags = ntohl(resolver->flags);

	// initialize SCNetworkReachability flags

	resolver->reach_flags = ntohl(resolver->reach_flags);

	// process resolver buffer "attribute" data

	n_attribute = n_buf - sizeof(_dns_resolver_buf_t);
	/* ALIGN: alignment not assumed, using accessors */
	attribute = (dns_attribute_t *)(void *)&buf->attribute[0];
	if (n_attribute != ntohl(buf->n_attribute)) {
		goto error;
	}

	while (n_attribute >= sizeof(dns_attribute_t)) {
		uint32_t	attribute_length	= ntohl(attribute->length);

		switch (ntohl(attribute->type)) {
			case RESOLVER_ATTRIBUTE_DOMAIN :
				resolver->domain = (char *)&attribute->attribute[0];
				break;

			case RESOLVER_ATTRIBUTE_ADDRESS :
				if (resolver->nameserver == NULL) {
					goto error;
				}
				resolver->nameserver[n_nameserver++] = (struct sockaddr *)&attribute->attribute[0];
				break;

			case RESOLVER_ATTRIBUTE_SEARCH :
				if (resolver->search == NULL) {
					goto error;
				}
				resolver->search[n_search++] = (char *)&attribute->attribute[0];
				break;

			case RESOLVER_ATTRIBUTE_SORTADDR :
				if (resolver->sortaddr == NULL) {
					goto error;
				}
				resolver->sortaddr[n_sortaddr++] = (dns_sortaddr_t *)(void *)&attribute->attribute[0];
				break;

			case RESOLVER_ATTRIBUTE_OPTIONS :
				resolver->options = (char *)&attribute->attribute[0];
				break;

			case RESOLVER_ATTRIBUTE_CONFIGURATION_ID :
				resolver->cid = (char *)&attribute->attribute[0];
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


/*
 * expand a DNS "configuration" from the provided buffer
 */
static __inline__ dns_config_t *
_dns_configuration_expand_config(_dns_config_buf_t *buf)
{
	dns_attribute_t		*attribute;
	dns_config_t		*config			= (dns_config_t *)buf;
	uint32_t		n_attribute;
	uint32_t		n_padding;
	int32_t			n_resolver		= 0;
	int32_t			n_scoped_resolver	= 0;
	int32_t			n_service_specific_resolver	= 0;
	void			*padding;

	n_attribute = ntohl(buf->n_attribute);	// pre-validated (or known OK) at entry
	n_padding   = ntohl(buf->n_padding);	// pre-validated (or known OK) at entry

	// establish the start of padding to be after the last attribute

	padding = &buf->attribute[n_attribute];

	// initialize resolver lists

	config->n_resolver = ntohl(config->n_resolver);
	if (!__dns_configuration_expand_add_list(&padding,
						 &n_padding,
						 config->n_resolver,
						 sizeof(DNS_PTR(dns_resolver_t *, x)),
						 (void **)&config->resolver)) {
		goto error;
	}

	config->n_scoped_resolver = ntohl(config->n_scoped_resolver);
	if (!__dns_configuration_expand_add_list(&padding,
						 &n_padding,
						 config->n_scoped_resolver,
						 sizeof(DNS_PTR(dns_resolver_t *, x)),
						 (void **)&config->scoped_resolver)) {
		goto error;
	}

	config->n_service_specific_resolver = ntohl(config->n_service_specific_resolver);
	if (!__dns_configuration_expand_add_list(&padding,
						 &n_padding,
						 config->n_service_specific_resolver,
						 sizeof(DNS_PTR(dns_resolver_t *, x)),
						 (void **)&config->service_specific_resolver)) {
		goto error;
	}

	// process configuration buffer "attribute" data

	attribute = (dns_attribute_t *)(void *)&buf->attribute[0];

	while (n_attribute >= sizeof(dns_attribute_t)) {
		uint32_t	attribute_length	= ntohl(attribute->length);
		uint32_t	attribute_type		= ntohl(attribute->type);

		switch (attribute_type) {
			case CONFIG_ATTRIBUTE_RESOLVER :
			case CONFIG_ATTRIBUTE_SCOPED_RESOLVER   :
			case CONFIG_ATTRIBUTE_SERVICE_SPECIFIC_RESOLVER : {
				dns_resolver_t	*resolver;

				// expand resolver buffer

				resolver = _dns_configuration_expand_resolver((_dns_resolver_buf_t *)(void *)&attribute->attribute[0],
							   attribute_length - sizeof(dns_attribute_t),
							   &padding,
							   &n_padding);
				if (resolver == NULL) {
					goto error;
				}

				// add resolver to config list

				if (attribute_type == CONFIG_ATTRIBUTE_RESOLVER) {
					if (config->resolver == NULL) {
						goto error;
					}
					config->resolver[n_resolver++] = resolver;
				} else if (attribute_type == CONFIG_ATTRIBUTE_SCOPED_RESOLVER) {
					if (config->scoped_resolver == NULL) {
						goto error;
					}
					config->scoped_resolver[n_scoped_resolver++] = resolver;
				} else if (attribute_type == CONFIG_ATTRIBUTE_SERVICE_SPECIFIC_RESOLVER) {
					if (config->service_specific_resolver == NULL) {
						goto error;
					}
					config->service_specific_resolver[n_service_specific_resolver++] = resolver;
				}

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

	if (n_scoped_resolver != config->n_scoped_resolver) {
		goto error;
	}

	if (n_service_specific_resolver != config->n_service_specific_resolver) {
		goto error;
	}

	return config;

    error :

	return NULL;
}


static __inline__ void
_dns_resolver_log(dns_resolver_t *resolver, int index, Boolean debug)
{
	int			i;
	uint32_t		flags;
	CFMutableStringRef	str;

	my_log(LOG_INFO, " ");
	my_log(LOG_INFO, "resolver #%d", index);

	if (resolver->domain != NULL) {
		my_log(LOG_INFO, "  domain   : %s", resolver->domain);
	}

	for (i = 0; i < resolver->n_search; i++) {
		my_log(LOG_INFO, "  search domain[%d] : %s", i, resolver->search[i]);
	}

	for (i = 0; i < resolver->n_nameserver; i++) {
		char	buf[128];

		_SC_sockaddr_to_string(resolver->nameserver[i], buf, sizeof(buf));
		my_log(LOG_INFO, "  nameserver[%d] : %s", i, buf);
	}

	for (i = 0; i < resolver->n_sortaddr; i++) {
		char	abuf[32];
		char	mbuf[32];

		(void)inet_ntop(AF_INET, &resolver->sortaddr[i]->address, abuf, sizeof(abuf));
		(void)inet_ntop(AF_INET, &resolver->sortaddr[i]->mask,    mbuf, sizeof(mbuf));
		my_log(LOG_INFO, "  sortaddr[%d] : %s/%s", i, abuf, mbuf);
	}

	if (resolver->options != NULL) {
		my_log(LOG_INFO, "  options  : %s", resolver->options);
	}

	if (resolver->port != 0) {
		my_log(LOG_INFO, "  port     : %hd", resolver->port);
	}

	if (resolver->timeout != 0) {
		my_log(LOG_INFO, "  timeout  : %d", resolver->timeout);
	}

	if (resolver->if_index != 0) {
		char	buf[IFNAMSIZ];
		char	*if_name;

		if_name = if_indextoname(resolver->if_index, buf);
		my_log(LOG_INFO, "  if_index : %d (%s)",
			resolver->if_index,
			(if_name != NULL) ? if_name : "?");
	}

	if (resolver->service_identifier != 0) {
		my_log(LOG_INFO, "  service_identifier : %d",
			resolver->service_identifier);
	}

	flags = resolver->flags;
	str = CFStringCreateMutable(NULL, 0);
	CFStringAppend(str, CFSTR("  flags    : "));
	if (debug) {
		CFStringAppendFormat(str, NULL, CFSTR("0x%08x ("), flags);
	}
	if (flags & DNS_RESOLVER_FLAGS_SCOPED) {
		flags &= ~DNS_RESOLVER_FLAGS_SCOPED;
		CFStringAppendFormat(str, NULL, CFSTR("Scoped%s"), flags != 0 ? ", " : "");
	}
	if (flags & DNS_RESOLVER_FLAGS_SERVICE_SPECIFIC) {
		flags &= ~DNS_RESOLVER_FLAGS_SERVICE_SPECIFIC;
		CFStringAppendFormat(str, NULL, CFSTR("Service-specific%s"), flags != 0 ? ", " : "");
	}
	if (flags & DNS_RESOLVER_FLAGS_SUPPLEMENTAL) {
		flags &= ~DNS_RESOLVER_FLAGS_SUPPLEMENTAL;
		CFStringAppendFormat(str, NULL, CFSTR("Supplemental%s"), flags != 0 ? ", " : "");
	}
	if (flags & DNS_RESOLVER_FLAGS_REQUEST_A_RECORDS) {
		flags &= ~DNS_RESOLVER_FLAGS_REQUEST_A_RECORDS;
		CFStringAppendFormat(str, NULL, CFSTR("Request A records%s"), flags != 0 ? ", " : "");
	}
	if (flags & DNS_RESOLVER_FLAGS_REQUEST_AAAA_RECORDS) {
		flags &= ~DNS_RESOLVER_FLAGS_REQUEST_AAAA_RECORDS;
		CFStringAppendFormat(str, NULL, CFSTR("Request AAAA records%s"), flags != 0 ? ", " : "");
	}
	if (flags != 0) {
		CFStringAppendFormat(str, NULL, CFSTR("0x%08x"), flags);
	}
	if (debug) {
		CFStringAppend(str, CFSTR(")"));
	}
	my_log(LOG_INFO, "%@", str);
	CFRelease(str);

	str = (CFMutableStringRef)__SCNetworkReachabilityCopyFlags(resolver->reach_flags,
								   CFSTR("  reach    : "),
								   debug);
	my_log(LOG_INFO, "%@", str);
	CFRelease(str);

	if (resolver->search_order != 0) {
		my_log(LOG_INFO, "  order    : %d", resolver->search_order);
	}

	if (debug && (resolver->cid != NULL)) {
		my_log(LOG_INFO, "  config id: %s", resolver->cid);
	}

	return;
}


static __inline__ void
_dns_configuration_log(dns_config_t *dns_config, Boolean debug)
{
	int	i;

	my_log(LOG_INFO, "DNS configuration");

	for (i = 0; i < dns_config->n_resolver; i++) {
		dns_resolver_t	*resolver	= dns_config->resolver[i];

		_dns_resolver_log(resolver, i + 1, debug);
	}

	if ((dns_config->n_scoped_resolver > 0) && (dns_config->scoped_resolver != NULL)) {
		my_log(LOG_INFO, " ");
		my_log(LOG_INFO, "DNS configuration (for scoped queries)");

		for (i = 0; i < dns_config->n_scoped_resolver; i++) {
			dns_resolver_t	*resolver	= dns_config->scoped_resolver[i];

			_dns_resolver_log(resolver, i + 1, debug);
		}
	}

	if ((dns_config->n_service_specific_resolver > 0) && (dns_config->service_specific_resolver != NULL)) {
		my_log(LOG_INFO, " ");
		my_log(LOG_INFO, "DNS configuration (for service-specific queries)");

		for (i = 0; i < dns_config->n_service_specific_resolver; i++) {
			dns_resolver_t	*resolver	= dns_config->service_specific_resolver[i];

			_dns_resolver_log(resolver, i + 1, debug);
		}
	}

	return;
}


#ifdef	MY_LOG_DEFINED_LOCALLY
#undef	my_log
#undef	MY_LOG_DEFINED_LOCALLY
#endif	// MY_LOG_DEFINED_LOCALLY


__END_DECLS

#endif	/* !_S_DNSINFO_INTERNAL_H */
