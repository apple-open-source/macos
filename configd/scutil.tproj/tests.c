/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
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

#include <SystemConfiguration/SCPrivate.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>


void
do_checkReachability(char *node)
{
	SCNetworkConnectionFlags	flags	= 0;
	Boolean				ok	= FALSE;

	if (isdigit(node[0])) {
		struct sockaddr_in	sin;

		bzero(&sin, sizeof(sin));
		sin.sin_len         = sizeof(sin);
		sin.sin_family      = AF_INET;
		sin.sin_addr.s_addr = inet_addr(node);
		if (inet_aton(node, &sin.sin_addr) == 0) {
			SCPrint(TRUE, stderr, CFSTR("Could not interpret address \"%s\"\n"), node);
			exit(1);
		}
		ok = SCNetworkCheckReachabilityByAddress((struct sockaddr *)&sin,
							 sizeof(sin),
							 &flags);

	} else {
		ok = SCNetworkCheckReachabilityByName(node,
						      &flags);
	}

	if (!ok) {
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
