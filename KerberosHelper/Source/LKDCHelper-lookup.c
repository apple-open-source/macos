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


#include <dns_sd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "LKDCHelper.h"
#include "LKDCHelper-lookup.h"

typedef struct _lookupContext {
	LKDCLocator	*realm;
	volatile int	terminateLoop;
	volatile DNSServiceErrorType errorCode;	
} lookupContext;

// Lookup timeout for LKDC.  The following constant is certain to be "wrong"
#define LKDC_DNS_TIMEOUT	11

static LKDCHelperErrorType HandleEvents(DNSServiceRef serviceRef, lookupContext *context)
{
	LKDCHelperErrorType err = kLKDCHelperSuccess;
	int			dns_sd_fd, nfds, result;
	fd_set		readfds;
	struct		timeval tv;

	LKDCLogEnter ();

	dns_sd_fd = DNSServiceRefSockFD(serviceRef);
	nfds = dns_sd_fd + 1;
	context->terminateLoop = 0;
	context->errorCode = 0;
	
	while (!context->terminateLoop) {
		FD_ZERO(&readfds);
		FD_SET(dns_sd_fd, &readfds);

		tv.tv_sec = LKDC_DNS_TIMEOUT;
		tv.tv_usec = 0;
		
		result = select(nfds, &readfds, NULL, NULL, &tv);
		if (result > 0) {
			if (FD_ISSET(dns_sd_fd, &readfds)) {
				LKDCLog ("mDNSResult");
				err = DNSServiceProcessResult(serviceRef);
				if (kDNSServiceErr_NoSuchRecord == context->errorCode) {
					err = kLKDCHelperRealmNotFound;
				}
			}
			if (err || context->errorCode) {
				LKDCLog ("mDNSError = %d", err);
				LKDCLog ("CallbackError = %d", context->errorCode);
				context->terminateLoop = 1;
			}
		} else if (result < 0 && errno == EINTR) {
			/* Try again */

		} else {
			/* Timeout or other error */
			LKDCLog ("Timeout!");
			err = kLKDCHelperRealmNotFound;
			context->terminateLoop = 1;
		}
	}

	LKDCLogExit (err);
	return err;
}

static const char		*kerberosService = "_kerberos";
/* static const char		*kerberosServiceName = "kerberos"; */
/* static const char		*kerberosServiceType = "_kerberos._udp."; */
static const uint16_t	kerberosServicePort = 88;
/* static const char		*kerberosServicePortString = "88"; */

static void LookupRealmCallBack(
					DNSServiceRef serviceRef,
					DNSServiceFlags flags,
					uint32_t interface,
					DNSServiceErrorType errorCode,
					const char *fullname,
					uint16_t rrType,
					uint16_t rrClass,
					uint16_t rdlen,
					const void *rdata,
					uint32_t ttl,
					void *ctx
					)
{
	char *realm = NULL;
	uint32_t size = 0;
	lookupContext *context = (lookupContext *)ctx;
	
	context->errorCode = errorCode;
	if (kDNSServiceErr_NoSuchRecord == errorCode) {
		context->terminateLoop = 1;
	} else if (errorCode != kDNSServiceErr_NoError) {
		/* We'll try again */
		LKDCLog ("mDNSError = %d", errorCode);
	} else {
		if (rdlen > 1 && rdlen < 1024 /* max realm name? */) {
			size = *(const unsigned char *)rdata;
			if (size >= rdlen) /* XXX bad yo */;
			realm = malloc (size + 1);
			
			memcpy (realm, rdata + 1, size);
			realm[size] = '\0';
			
			context->realm->realmName = realm;

			if (NULL != fullname) { context->realm->serviceHost = fullname; }

			context->realm->servicePort = kerberosServicePort;

			context->realm->ttl = ttl;
			context->realm->absoluteTTL = time(NULL) + ttl;
			
			if (flags & kDNSServiceFlagsMoreComing) {
				LKDCLog ("More than one record, last one wins!!!");
			} else {
				context->terminateLoop = 1;
			}
		}
	}
}


static LKDCHelperErrorType LKDCLookupRealm (const char *hostname, LKDCLocator *l)
{
	// DNSServiceErrorType error = kDNSServiceErr_NoError;
	LKDCHelperErrorType error = kLKDCHelperSuccess;
	DNSServiceRef		serviceRef = NULL;
	lookupContext		context;
	char				*lookupName;
	
	LKDCLogEnter();
	
	if (NULL == hostname || NULL == l) { goto Done; }
	
	context.realm = l;
	
	asprintf (&lookupName, "%s.%s", kerberosService, hostname);
	
	if (NULL == lookupName) {
		error = kDNSServiceErr_NoMemory;
		goto Done;
	}
	
	error = DNSServiceQueryRecord (&serviceRef,
								   kDNSServiceFlagsReturnIntermediates,
								   0, // All network interfaces
								   lookupName,
								   kDNSServiceType_TXT,
								   kDNSServiceClass_IN,
								   &LookupRealmCallBack,
								   &context);
	
	if (kDNSServiceErr_NoError != error) { goto Done; }
	
	error = HandleEvents(serviceRef, &context);
	DNSServiceRefDeallocate(serviceRef);
	
	/* 
	 * "kdcmond" does not register SRV records, so we fake
	 * the serviceName and servicePort for now
	 */
	
	l->serviceHost = strdup (hostname);
	l->servicePort = kerberosServicePort;

Done:
	if (lookupName) { free (lookupName); }
		
	LKDCLogExit(error);
	return error;
}


LKDCHelperErrorType LKDCCreateLocator (LKDCLocator **locator)
{
	LKDCLocator *l;
	LKDCHelperErrorType error = kLKDCHelperSuccess;
	
	if (NULL == locator) { error = kLKDCHelperParameterError; goto Done; }

	l = (LKDCLocator *) malloc (sizeof (LKDCLocator));

	if (NULL == l) { error = kLKDCHelperParameterError; goto Done; }

	memset (l, 0, sizeof (*l));

	*locator = l;

Done:	
	return error;
}

LKDCHelperErrorType LKDCReleaseLocator (LKDCLocator **locator)
{
	LKDCLocator *l = NULL;
	LKDCHelperErrorType error = kLKDCHelperSuccess;

	if (NULL == locator || NULL == *locator) { error = kLKDCHelperParameterError; goto Done; }

	l = *locator;
	
	if (l->realmName)			{ free ((char *)l->realmName); }
	if (l->serviceHost)			{ free ((char *)l->serviceHost); }

	free (l);
	
	*locator = NULL;

Done:
	return error; 
}

static LKDCLocator *root = NULL;

LKDCHelperErrorType LKDCAddLocatorDetails (LKDCLocator *l)
{
	LKDCHelperErrorType error = kLKDCHelperSuccess;
	LKDCLocator **lp;

	LKDCLogEnter();

	if (NULL == l) { error = kLKDCHelperParameterError; goto Done; }

	/* If the realm is already in the cache, update it.
	 * Otherwise, push it on the list.
	 */
	for (lp = &root; *lp != NULL; lp = &((*lp)->next))
		if (0 == strcmp(l->realmName, (*lp)->realmName))
			break;
	if (NULL == *lp) {
		LKDCLog ("New entry for (realm=%s host=%s)", l->realmName, l->serviceHost);
		l->next = root;
		root = l;
	} else {
		LKDCLog ("Replacing existing entry (realm=%s host=%s) with (realm=%s host=%s)",
			(*lp)->realmName, (*lp)->serviceHost, l->realmName, l->serviceHost);
		l->next = (*lp)->next;
		LKDCReleaseLocator(lp);
		*lp = l;
	}

Done:
	LKDCLogExit(error);
	return error; 
}


LKDCHelperErrorType LKDCHostnameForRealm (const char *realm, LKDCLocator **l)
{
	LKDCLocator *p;
	LKDCHelperErrorType error = kLKDCHelperSuccess;
	time_t now = time(NULL);

	LKDCLogEnter();

	if (NULL == l || NULL == realm) { error = kLKDCHelperParameterError; goto Done; }

	for (p = root; p != NULL; p = p->next) {
		if (strcmp (p->realmName, realm) == 0 && p->absoluteTTL > now) {
			LKDCLog ("Cache hit: %lus left", (unsigned long)(p->absoluteTTL - now));
			goto Found;
		}
	}

	LKDCLog ("Cache miss");

	p = NULL;

	/* There isn't anything we can do to map an arbitrary LocalKDC realm to a hostname */
	error = kLKDCHelperNoKDCForRealm;
	goto Done;
	
Found:
	*l = p;
	p = NULL;
	
Done:
	LKDCLogExit(error);
	return error; 
}

LKDCHelperErrorType LKDCRealmForHostname (const char *hostname, LKDCLocator **l)
{
	LKDCLocator *p = NULL;
	LKDCHelperErrorType error = kLKDCHelperSuccess;
	time_t now = time(NULL);

	LKDCLogEnter();

	if (NULL == l || NULL == hostname) { error = kLKDCHelperParameterError; goto Done; }
	
	for (p = root; p != NULL; p = p->next) {
		if (strcasecmp (p->serviceHost, hostname) == 0 && p->absoluteTTL > now) {
			LKDCLog ("Cache hit: %lus left", (unsigned long)(p->absoluteTTL - now));
			goto Found;
		}
	}
	
	LKDCLog ("Cache miss");
	
	p = NULL;
	
	error = LKDCCreateLocator (&p);
	if (kLKDCHelperSuccess != error) { goto Done; }
		 
	error = LKDCLookupRealm (hostname, p);
	if (kLKDCHelperSuccess != error) { error = kLKDCHelperRealmNotFound; goto Done; }

	error = LKDCAddLocatorDetails (p);
	
Found:
	*l = p;
	p = NULL;

Done:
	if (NULL != p) { LKDCReleaseLocator (&p); }

	LKDCLogExit(error);
	return error; 
}

LKDCHelperErrorType LKDCDumpCacheStatus ()
{
	LKDCLocator *p = NULL;
	LKDCHelperErrorType error = kLKDCHelperSuccess;
	
	LKDCLog ("Cache root node = %08p", root);

	for (p = root; p != NULL; p = p->next) {
		LKDCLog ("node = %08p {", p);
		LKDCLog ("                 realmName   = (%08p) %s", p->realmName, p->realmName);
		LKDCLog ("                 serviceHost = (%08p) %s", p->serviceHost,  p->serviceHost);
		LKDCLog ("                 servicePort = %u", p->servicePort);
		LKDCLog ("                 TTL         = %u", p->ttl);
		LKDCLog ("                }");
	}

	return error; 
}

