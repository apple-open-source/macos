/*
 *  LKDC-lookup-plugin.c
 *  LocalKDC
 */

/*
 * Copyright (c) 2006-2007 Apple Inc. All rights reserved.
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

#include "LKDC-lookup-plugin.h"
#include <asl.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <Kerberos/krb5.h>
#include <Kerberos/locate_plugin.h>

#include "LKDCHelper.h"

typedef struct _state {
	krb5_context krb_context;
	time_t	cacheCreateTime;
} state;

#define LKDC_PLUGIN_DEBUG 1
#if LKDC_PLUGIN_DEBUG
#define debug(fmt, ...) do { debug_(__func__, fmt, ## __VA_ARGS__); } while(0)
static inline void
debug_(const char *func, const char *fmt, ...)
{
	static char ellipsis[] = "[...]";
	char buf[2048];
	char *p = buf;
	char *endp = &buf[sizeof(buf)];
	ssize_t n = snprintf(p, endp-p, "%s: ", func);
	
	if ((size_t)n >= endp-p)
		return;
	p += n;
	va_list ap;
	va_start(ap, fmt);
	n = vsnprintf(p, endp-p, fmt, ap);
	va_end(ap);
	if ((size_t)n >= endp-p)
		snprintf(endp-sizeof(ellipsis), sizeof(ellipsis), ellipsis);
	asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "%s", buf);
}
#else
#define debug(...)
#endif

static krb5_error_code LKDCInit (krb5_context c, void **ptr)
{
	return 0;
}

static void LKDCFinish (void *ptr) {
	return;
}

static krb5_error_code LKDCLookup (void *ptr,
								   enum locate_service_type svc,
								   const char *realm,
								   int socktype,
								   int family,
								   int (*cbfunc)(void *, int, struct sockaddr *),
								   void *cbdata)
{
	krb5_error_code error = KRB5_PLUGIN_NO_HANDLE;
	char			*hostname = NULL;
	struct addrinfo  aiHints;
	struct addrinfo *aiResult = NULL;
	struct addrinfo *res = NULL;
	uint16_t		port;
	int				err = 0;

	switch (family) {
	case 0:
	case AF_INET:
	case AF_INET6:
		break;
	default:
		debug("Declined to handle address family %d", family);
		return error;
	}

	debug("svc = %d, realm = %s, family= %d, socktype = %d", svc, realm, family, socktype);
	switch (svc) {
		case locate_service_kdc:
		case locate_service_master_kdc:
			break;
		case locate_service_kadmin:
		case locate_service_krb524:
		case locate_service_kpasswd:
		default:
			return error;
	}
	debug("KDC|MasterKDC");
	
	/* Do we handle this type of realm? */
	if (strncmp ("LKDC:", realm, 5) != 0) {
		return error;
	}
		
	err = LKDCFindKDCForRealm (realm, &hostname, &port);
	if (0 != err || NULL == hostname) {
		return error;
	}
	
	bzero (&aiHints, sizeof (aiHints));
	aiHints.ai_flags = 0;
	aiHints.ai_family = family;
	aiHints.ai_socktype = socktype;
	
	err = getaddrinfo (hostname,
					   NULL,	// realmDetails->servicePortName?
					   &aiHints,
					   &aiResult);
	debug("getaddrinfo () == %d", err);
	
	if (0 == err) {
		for (res = aiResult; res != NULL; res = res->ai_next) {
			void *in_addr = NULL;
			uint16_t in_port = 0;
			
			debug("0x%08p: family = %d, socktype = %d, protocol = %d", res, res->ai_family, res->ai_socktype, res->ai_protocol);
			debug("Running callback 0x%08p", res);
			switch (res->ai_family) {
			case AF_INET:
				in_addr = &(((struct sockaddr_in *)res->ai_addr)->sin_addr);
				((struct sockaddr_in *)res->ai_addr)->sin_port = htons (in_port = port);
				break;
			case AF_INET6:
				in_addr = &(((struct sockaddr_in6 *)res->ai_addr)->sin6_addr);
				((struct sockaddr_in6 *)res->ai_addr)->sin6_port = htons (in_port = port);
				break;
			default:
				in_addr = NULL;
				debug("Unexpected address family %d", res->ai_family);
				break;
			}
			if (NULL != in_addr) {
				err = cbfunc (cbdata, res->ai_socktype, res->ai_addr);
				debug("Callback done 0x%08p, err=%d", res, err);
#if LKDC_PLUGIN_DEBUG
				{
					char ipString[1024];
					ipString[0] = '\0';
					if (NULL == inet_ntop(res->ai_family, in_addr, ipString, sizeof(ipString)))
						debug("inet_ntop failed: %s", strerror(errno));
					else
						debug("addr = %s, port = %d", ipString, (int)in_port);
				}
#endif
			}

		}
		freeaddrinfo (aiResult);
		aiResult = NULL;
	} else {
		debug("failed %d", error);
		return error;
	}

	debug("OK");
	return 0;
}

/* Structure used by Kerberos to locate the functions to call for this plugin. */

const krb5plugin_service_locate_ftable service_locator = {
    0,	/* version */
	LKDCInit,		/* Initialize - called each time */
	LKDCFinish,		/* Finish - called each time */
	LKDCLookup,		/* Lookup function */
};
