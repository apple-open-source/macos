/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * July 9, 2001			Allan Nathanson <ajn@apple.com>
 * - added "-r" option for checking network reachability
 * - added "-w" option to check/wait for the presence of a
 *   dynamic store key.
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * November 9, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include "scutil.h"
#include "tests.h"

#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>


void
do_checkReachability(int argc, char **argv)
{
	SCNetworkConnectionFlags	flags	= 0;
	SCNetworkReachabilityRef	target	= NULL;

	if (argc == 1) {
		struct sockaddr_in	sin;
		struct sockaddr_in6	sin6;

		bzero(&sin, sizeof(sin));
		sin.sin_len         = sizeof(sin);
		sin.sin_family      = AF_INET;

		bzero(&sin6, sizeof(sin6));
		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_family = AF_INET6;

		if (inet_aton(argv[0], &sin.sin_addr) == 1) {
			target = SCNetworkReachabilityCreateWithAddress(NULL, (struct sockaddr *)&sin);
		} else if (inet_pton(AF_INET6, argv[0], &sin6.sin6_addr) == 1) {
			char	*p;

			p = strchr(argv[0], '%');
			if (p != NULL) {
				sin6.sin6_scope_id = if_nametoindex(p+1);
			}

			target = SCNetworkReachabilityCreateWithAddress(NULL, (struct sockaddr *)&sin6);
		} else {
			target = SCNetworkReachabilityCreateWithName(NULL, argv[0]);
		}
	} else /* if (argc == 2) */ {
		struct sockaddr_in	l_sin;
		struct sockaddr_in	r_sin;

		bzero(&l_sin, sizeof(l_sin));
		l_sin.sin_len         = sizeof(l_sin);
		l_sin.sin_family      = AF_INET;
		if (inet_aton(argv[0], &l_sin.sin_addr) == 0) {
			SCPrint(TRUE, stderr, CFSTR("Could not interpret address \"%s\"\n"), argv[0]);
			exit(1);
		}

		bzero(&r_sin, sizeof(r_sin));
		r_sin.sin_len         = sizeof(r_sin);
		r_sin.sin_family      = AF_INET;
		if (inet_aton(argv[1], &r_sin.sin_addr) == 0) {
			SCPrint(TRUE, stderr, CFSTR("Could not interpret address \"%s\"\n"), argv[1]);
			exit(1);
		}

		target = SCNetworkReachabilityCreateWithAddressPair(NULL,
								    (struct sockaddr *)&l_sin,
								    (struct sockaddr *)&r_sin);
	}

	if (!target) {
		SCPrint(TRUE, stderr, CFSTR("  Could not determine status: %s\n"), SCErrorString(SCError()));
		exit(1);
	}

	if (!SCNetworkReachabilityGetFlags(target, &flags)) {
		SCPrint(TRUE, stderr, CFSTR("  Could not determine status: %s\n"), SCErrorString(SCError()));
		exit(1);
	}

	SCPrint(_sc_debug, stdout, CFSTR("flags = 0x%x"), flags);
	if (flags != 0) {
		SCPrint(_sc_debug, stdout, CFSTR(" ("));
		if (flags & kSCNetworkFlagsReachable) {
			SCPrint(TRUE, stdout, CFSTR("Reachable"));
			flags &= ~kSCNetworkFlagsReachable;
			SCPrint(flags != 0, stdout, CFSTR(","));
		}
		if (flags & kSCNetworkFlagsTransientConnection) {
			SCPrint(TRUE, stdout, CFSTR("Transient Connection"));
			flags &= ~kSCNetworkFlagsTransientConnection;
			SCPrint(flags != 0, stdout, CFSTR(","));
		}
		if (flags & kSCNetworkFlagsConnectionRequired) {
			SCPrint(TRUE, stdout, CFSTR("Connection Required"));
			flags &= ~kSCNetworkFlagsConnectionRequired;
			SCPrint(flags != 0, stdout, CFSTR(","));
		}
		if (flags & kSCNetworkFlagsConnectionAutomatic) {
			SCPrint(TRUE, stdout, CFSTR("Connection Automatic"));
			flags &= ~kSCNetworkFlagsConnectionAutomatic;
			SCPrint(flags != 0, stdout, CFSTR(","));
		}
		if (flags & kSCNetworkFlagsInterventionRequired) {
			SCPrint(TRUE, stdout, CFSTR("Intervention Required"));
			flags &= ~kSCNetworkFlagsInterventionRequired;
			SCPrint(flags != 0, stdout, CFSTR(","));
		}
		if (flags & kSCNetworkFlagsIsLocalAddress) {
			SCPrint(TRUE, stdout, CFSTR("Local Address"));
			flags &= ~kSCNetworkFlagsIsLocalAddress;
			SCPrint(flags != 0, stdout, CFSTR(","));
		}
		if (flags & kSCNetworkFlagsIsDirect) {
			SCPrint(TRUE, stdout, CFSTR("Directly Reachable Address"));
			flags &= ~kSCNetworkFlagsIsDirect;
			SCPrint(flags != 0, stdout, CFSTR(","));
		}
		SCPrint(_sc_debug, stdout, CFSTR(")"));
	} else {
		SCPrint(_sc_debug, stdout, CFSTR(" ("));
		SCPrint(TRUE, stdout, CFSTR("Not Reachable"));
		SCPrint(_sc_debug, stdout, CFSTR(")"));
	}
	SCPrint(TRUE, stdout, CFSTR("\n"));
	exit(0);
}


void
do_snapshot(int argc, char **argv)
{
	if (!SCDynamicStoreSnapshot(store)) {
		SCPrint(TRUE, stdout, CFSTR("  %s\n"), SCErrorString(SCError()));
	}
	return;
}


static void
waitKeyFound()
{
	exit(0);
}


static void
waitTimeout(int sigraised)
{
	exit(1);
}


void
do_wait(char *waitKey, int timeout)
{
	struct itimerval	itv;
	CFStringRef		key;
	CFMutableArrayRef	keys;

	store = SCDynamicStoreCreate(NULL, CFSTR("scutil (wait)"), waitKeyFound, NULL);
	if (!store) {
		SCPrint(TRUE, stderr,
			CFSTR("SCDynamicStoreCreate() failed: %s\n"), SCErrorString(SCError()));
		exit(1);
	}

	keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	key  = CFStringCreateWithCString(NULL, waitKey, kCFStringEncodingMacRoman);
	CFArrayAppendValue(keys, key);

	if (!SCDynamicStoreSetNotificationKeys(store, keys, NULL)) {
		SCPrint(TRUE, stderr,
			CFSTR("SCDynamicStoreSetNotificationKeys() failed: %s\n"), SCErrorString(SCError()));
		exit(1);
	}

	notifyRls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
	if (!notifyRls) {
		SCPrint(TRUE, stderr,
			CFSTR("SCDynamicStoreCreateRunLoopSource() failed: %s\n"), SCErrorString(SCError()));
		exit(1);
	}

	CFRunLoopAddSource(CFRunLoopGetCurrent(), notifyRls, kCFRunLoopDefaultMode);

	value = SCDynamicStoreCopyValue(store, key);
	if (value) {
		/* if the key is already present */
		exit(0);
	}
	CFRelease(key);

	if (waitTimeout > 0) {
		signal(SIGALRM, waitTimeout);
		bzero(&itv, sizeof(itv));
		itv.it_value.tv_sec = timeout;
		if (setitimer(ITIMER_REAL, &itv, NULL) < 0) {
			SCPrint(TRUE, stderr,
				CFSTR("setitimer() failed: %s\n"), strerror(errno));
			exit(1);
		}
	}

	CFRunLoopRun();
}
