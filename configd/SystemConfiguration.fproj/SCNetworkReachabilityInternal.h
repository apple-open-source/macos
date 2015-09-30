/*
 * Copyright (c) 2003-2015 Apple Inc. All rights reserved.
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

#ifndef _SCNETWORKREACHABILITYINTERNAL_H
#define _SCNETWORKREACHABILITYINTERNAL_H

#include <Availability.h>
#include <TargetConditionals.h>
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <dispatch/dispatch.h>

#include <dns_sd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <net/if.h>
#include <xpc/xpc.h>

#include <network/private.h>

#pragma mark -
#pragma mark SCNetworkReachability

#define kSCNetworkReachabilityFlagsMask			0x00ffffff	// top 8-bits reserved for implementation


typedef	enum {
	NO	= 0,
	YES,
	UNKNOWN
} lazyBoolean;


typedef enum {
	// by-address SCNetworkReachability targets
	reachabilityTypeAddress,
	reachabilityTypeAddressPair,
	// by-name SCNetworkReachability targets
	reachabilityTypeName,
	reachabilityTypePTR
} ReachabilityAddressType;

#define isReachabilityTypeAddress(type)		(type < reachabilityTypeName)
#define isReachabilityTypeName(type)		(type >= reachabilityTypeName)

typedef struct {

	/* base CFType information */
	CFRuntimeBase			cfBase;

	/* lock */
	pthread_mutex_t			lock;

	/* address type */
	ReachabilityAddressType		type;

	/* target host name */
	nw_endpoint_t			hostnameEndpoint;

	/* local & remote addresses */
	nw_endpoint_t			localAddressEndpoint;
	nw_endpoint_t			remoteAddressEndpoint;

	/* run loop source, callout, context, rl scheduling info */
	Boolean				scheduled;
	Boolean				sentFirstUpdate;
	CFRunLoopSourceRef		rls;
	SCNetworkReachabilityCallBack	rlsFunction;
	SCNetworkReachabilityContext	rlsContext;
	CFMutableArrayRef		rlList;

	dispatch_queue_t		dispatchQueue;		// SCNetworkReachabilitySetDispatchQueue

	Boolean				resolverBypass;		// set this flag to bypass resolving the name

	/* logging */
	char				log_prefix[32];

	nw_parameters_t			parameters;
	nw_path_evaluator_t		pathEvaluator;
	nw_path_t			lastPath;
	nw_parameters_t			lastPathParameters;
	nw_resolver_t			resolver;
	nw_resolver_status_t		lastResolverStatus;
	nw_array_t			lastResolvedEndpoints;
	Boolean				lastResolvedEndpointHasFlags;
	SCNetworkReachabilityFlags	lastResolvedEndpointFlags;
	uint				lastResolvedEndpointInterfaceIndex;

} SCNetworkReachabilityPrivate, *SCNetworkReachabilityPrivateRef;


// ------------------------------------------------------------


__BEGIN_DECLS

CFStringRef
_SCNetworkReachabilityCopyTargetDescription	(SCNetworkReachabilityRef	target);


static __inline__ CFStringRef
__SCNetworkReachabilityCopyFlags(SCNetworkReachabilityFlags flags, CFStringRef prefix, Boolean debug)
{
	CFMutableStringRef	str	= CFStringCreateMutable(NULL, 0);

	if (debug) {
		if (prefix != NULL) {
			CFStringAppend(str, prefix);
		}
		
		CFStringAppendFormat(str, NULL, CFSTR("0x%08x ("), flags);
	}

	if (flags == 0) {
		CFStringAppend(str, CFSTR("Not Reachable"));
	}
	if (flags & kSCNetworkReachabilityFlagsReachable) {
		flags &= ~kSCNetworkReachabilityFlagsReachable;
		CFStringAppendFormat(str, NULL, CFSTR("Reachable%s"),
				     flags != 0 ? ", " : "");
	}
	if (flags & kSCNetworkReachabilityFlagsTransientConnection) {
		flags &= ~kSCNetworkReachabilityFlagsTransientConnection;
		CFStringAppendFormat(str, NULL, CFSTR("Transient Connection%s"),
				     flags != 0 ? ", " : "");
	}
	if (flags & kSCNetworkReachabilityFlagsConnectionRequired) {
		flags &= ~kSCNetworkReachabilityFlagsConnectionRequired;
		CFStringAppendFormat(str, NULL, CFSTR("Connection Required%s"),
				     flags != 0 ? ", " : "");
	}
	if (flags & kSCNetworkReachabilityFlagsConnectionOnTraffic) {
		flags &= ~kSCNetworkReachabilityFlagsConnectionOnTraffic;
		CFStringAppendFormat(str, NULL, CFSTR("Automatic Connection On Traffic%s"),
				     flags != 0 ? ", " : "");
	}
	if (flags & kSCNetworkReachabilityFlagsConnectionOnDemand) {
		flags &= ~kSCNetworkReachabilityFlagsConnectionOnDemand;
		CFStringAppendFormat(str, NULL, CFSTR("Automatic Connection On Demand%s"),
				     flags != 0 ? ", " : "");
	}
	if (flags & kSCNetworkReachabilityFlagsInterventionRequired) {
		flags &= ~kSCNetworkReachabilityFlagsInterventionRequired;
		CFStringAppendFormat(str, NULL, CFSTR("Intervention Required%s"),
				     flags != 0 ? ", " : "");
	}
	if (flags & kSCNetworkReachabilityFlagsIsLocalAddress) {
		flags &= ~kSCNetworkReachabilityFlagsIsLocalAddress;
		CFStringAppendFormat(str, NULL, CFSTR("Local Address%s"),
				     flags != 0 ? ", " : "");
	}
	if (flags & kSCNetworkReachabilityFlagsIsDirect) {
		flags &= ~kSCNetworkReachabilityFlagsIsDirect;
		CFStringAppendFormat(str, NULL, CFSTR("Directly Reachable Address%s"),
				     flags != 0 ? ", " : "");
	}
#if	TARGET_OS_IPHONE
	if (flags & kSCNetworkReachabilityFlagsIsWWAN) {
		flags &= ~kSCNetworkReachabilityFlagsIsWWAN;
		CFStringAppendFormat(str, NULL, CFSTR("WWAN%s"), flags != 0 ? ", " : "");
	}
#endif	// TARGET_OS_IPHONE
	if (flags != 0) {
		CFStringAppendFormat(str, NULL, CFSTR("0x%08x"), flags);
	}

	if (debug) {
		CFStringAppend(str, CFSTR(")"));
	}

	return str;
}


__END_DECLS

#endif	// _SCNETWORKREACHABILITYINTERNAL_H
