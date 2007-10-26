/* 
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#include <sys/cdefs.h>
#include <arpa/inet.h>
#include <bsm/libbsm.h>
#include <asl.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <CoreFoundation/CoreFoundation.h>

#include "LKDCHelper-main.h"
#include "LKDCHelper.h"

/* MIG Generated file */
#include "LKDCHelperMessageServer.h"

#include "LKDCHelper-lookup.h"

kern_return_t
do_LKDCHelperExit (__unused mach_port_t port, audit_token_t token)
{
	if (!authorized(&token))
		goto fin;
	helplog(ASL_LEVEL_NOTICE, "Idle exit");
	exit(0);

fin:
	return KERN_SUCCESS;
}

kern_return_t
do_LKDCDumpStatus (__unused mach_port_t port, int logLevel, audit_token_t token)
{
	int				error = 0;
	int				savedLogLevel;

	LKDCLogEnter ();

	if (!authorized(&token))
		goto fin;

	savedLogLevel = LKDCLogLevel;
	LKDCLogLevel = logLevel;
	
	LKDCDumpCacheStatus ();

	LKDCLogLevel = savedLogLevel;

fin:
	LKDCLogExit (error);
	return KERN_SUCCESS;
}

kern_return_t
do_LKDCSetLogLevel (__unused mach_port_t port, int logLevel, audit_token_t token)
{
	int				error = 0;
	
	LKDCLogEnter ();
	
	if (!authorized(&token))
		goto fin;
	
	LKDCLogLevel = logLevel;
	
fin:
	LKDCLogExit (error);
	return KERN_SUCCESS;
}

kern_return_t
do_LKDCGetLocalRealm (__unused mach_port_t port, realmNameOut_t realm, int *err, audit_token_t token)
{
	CFStringRef		realmTmp = NULL;
	static char		*cachedLocalRealmString = NULL;
	int				error = 0;
	
	LKDCLogEnter ();

	if (NULL == cachedLocalRealmString) {
		error = DSCopyLocalKDC (&realmTmp);
	
		if (0 != error) { goto fin; }

		__KRBCreateUTF8StringFromCFString (realmTmp, &cachedLocalRealmString);
	} else {
		LKDCLog ("Cached lookup");
	}

	if (NULL != cachedLocalRealmString) {
		LKDCLog ("LocalKDCRealm = %s", cachedLocalRealmString);
		strlcpy (realm, cachedLocalRealmString, sizeof(realmNameOut_t));
	} else {
		*realm = '\0';
	}

fin:
	update_idle_timer();

	*err = error;
	LKDCLogExit (error);

	return KERN_SUCCESS;
}

kern_return_t
do_LKDCDiscoverRealm (__unused mach_port_t port, 
					  hostnameIn_t hostname,
					  realmNameOut_t realm,
					  int *err,
					  audit_token_t token)
{
	LKDCLocator    *lkdc;
	int				error = 0;

	LKDCLogEnter ();

	if (!authorized(&token)) {
		error = kLKDCHelperNotAuthorized;
		goto fin;
	}
	
	LKDCLog ("Looking up realm for %s", hostname);
	
	error = LKDCRealmForHostname (hostname, &lkdc);
	
	if (0 != error || NULL == lkdc->realmName) {
		goto fin;
	}
	
	strlcpy (realm, lkdc->realmName, sizeof (realmNameOut_t));

fin:
	update_idle_timer();

	*err = error;
	LKDCLogExit (error);

	return KERN_SUCCESS;
}

kern_return_t
do_LKDCFindKDCForRealm (__unused mach_port_t port, 
						realmNameIn_t realm,
						hostnameOut_t hostname,
						int *kdcport,
						int *err,
						audit_token_t token)
{
	LKDCLocator    *lkdc;
	int				error = 0;

	LKDCLogEnter ();

	if (!authorized(&token)) {
		error = kLKDCHelperNotAuthorized;
		goto fin;
	}
	
	LKDCLog ("Looking up host for %s", realm);

	error = LKDCHostnameForRealm (realm, &lkdc);
	
	if (0 != error || NULL == lkdc->serviceHost) {
		goto fin;
	}
	
	strlcpy (hostname, lkdc->serviceHost, sizeof (hostnameOut_t));
	*kdcport = lkdc->servicePort;

fin:
	update_idle_timer();
	
	*err = error;
	LKDCLogExit (error);
	
	return KERN_SUCCESS;
}
