/*
 * Copyright (c) 2003-2012 Apple Inc. All rights reserved.
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
#include <dispatch/dispatch.h>

#include <dns_sd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <net/if.h>

#if	((__MAC_OS_X_VERSION_MIN_REQUIRED >= 1080) || (__IPHONE_OS_VERSION_MIN_REQUIRED >= 50000)) && !TARGET_IPHONE_SIMULATOR
#define	HAVE_REACHABILITY_SERVER
#include <xpc/xpc.h>
#endif	// ((__MAC_OS_X_VERSION_MIN_REQUIRED >= 1080) || (__IPHONE_OS_VERSION_MIN_REQUIRED >= 50000)) && !TARGET_IPHONE_SIMULATOR


#pragma mark -
#pragma mark SCNetworkReachability


#define kSCNetworkReachabilityFlagsFirstResolvePending	(1<<31)


typedef	enum { NO = 0, YES, UNKNOWN }	lazyBoolean;


typedef enum {
	reachabilityTypeAddress,
	reachabilityTypeAddressPair,
	reachabilityTypeName
} ReachabilityAddressType;


typedef struct {
	uint64_t			cycle;
	SCNetworkReachabilityFlags	flags;
	unsigned int			if_index;
	char				if_name[IFNAMSIZ];
	Boolean				sleeping;
} ReachabilityInfo;


typedef struct {

	/* base CFType information */
	CFRuntimeBase			cfBase;

	/* lock */
	pthread_mutex_t			lock;

	/* address type */
	ReachabilityAddressType		type;

	/* target host name */
	const char			*name;
	const char			*serv;
	struct addrinfo			hints;
	Boolean				needResolve;
	CFArrayRef			resolvedAddress;	/* CFArray[CFData] */
	int				resolvedAddressError;

	/* [scoped routing] interface constraints */
	unsigned int			if_index;
	char				if_name[IFNAMSIZ];

	/* local & remote addresses */
	struct sockaddr			*localAddress;
	struct sockaddr			*remoteAddress;

	/* current reachability flags */
	uint64_t			cycle;
	ReachabilityInfo		info;
	ReachabilityInfo		last_notify;

	/* run loop source, callout, context, rl scheduling info */
	Boolean				scheduled;
	CFRunLoopSourceRef		rls;
	SCNetworkReachabilityCallBack	rlsFunction;
	SCNetworkReachabilityContext	rlsContext;
	CFMutableArrayRef		rlList;

	dispatch_group_t		dispatchGroup;
	dispatch_queue_t		dispatchQueue;		// SCNetworkReachabilitySetDispatchQueue

	/* [async] DNS query info */
	Boolean				haveDNS;
	mach_port_t			dnsMP;			// != MACH_PORT_NULL (if active)
	CFMachPortRef			dnsPort;		// for CFRunLoop queries
	CFRunLoopSourceRef		dnsRLS;			// for CFRunLoop queries
	dispatch_source_t		dnsSource;		// for dispatch queries
	struct timeval			dnsQueryStart;
	struct timeval			dnsQueryEnd;
	dispatch_source_t		dnsRetry;		// != NULL if DNS retry request queued
	int				dnsRetryCount;		// number of retry attempts

	/* [async] processing info */
	struct timeval			last_dns;
	struct timeval			last_network;
#if	!TARGET_OS_IPHONE
	struct timeval			last_power;
#endif	// !TARGET_OS_IPHONE
	struct timeval			last_push;

	/* on demand info */
	Boolean				onDemandBypass;
	CFStringRef			onDemandName;
	CFStringRef			onDemandRemoteAddress;
	SCNetworkReachabilityRef	onDemandServer;
	CFStringRef			onDemandServiceID;


	Boolean				llqActive;
	Boolean				llqBypass;
	DNSServiceRef			llqTarget;
	dispatch_source_t		llqTimer;		// != NULL while waiting for first callback

#ifdef	HAVE_REACHABILITY_SERVER
	/* SCNetworkReachability server "client" info */
	Boolean				serverActive;
	Boolean				serverBypass;
	Boolean				serverScheduled;
	ReachabilityInfo		serverInfo;

	/* SCNetworkReachability server "server" info */
	CFDataRef			serverDigest;
	dispatch_group_t		serverGroup;
	Boolean				serverInfoValid;
	unsigned int			serverQueryActive;	// 0 == no query active, else # waiting on group
	dispatch_queue_t		serverQueue;
	unsigned int			serverReferences;	// how many [client] targets
	CFMutableDictionaryRef		serverWatchers;		// [client_id/target_id] watchers
#endif	// HAVE_REACHABILITY_SERVER
	Boolean				resolverBypass;		// set this flag to bypass resolving the name

	/* logging */
	char				log_prefix[32];

} SCNetworkReachabilityPrivate, *SCNetworkReachabilityPrivateRef;


#ifdef	HAVE_REACHABILITY_SERVER

// ------------------------------------------------------------

#pragma mark -
#pragma mark [XPC] Reachability Server


#define	REACH_SERVER_VERSION		20110323
#define	REACH_SERVICE_NAME		"com.apple.SystemConfiguration.SCNetworkReachability"

// ------------------------------------------------------------


#pragma mark -
#pragma mark [XPC] Reachability Server (client->server request)


#define	REACH_CLIENT_PROC_NAME		"proc_name"		// string
#define	REACH_CLIENT_TARGET_ID		"target_id"		// uint64

#define	REACH_REQUEST			"request_op"		// int64

enum {
	REACH_REQUEST_CREATE		= 0x0001,
	REACH_REQUEST_REMOVE,
	REACH_REQUEST_SCHEDULE,
	REACH_REQUEST_STATUS,
	REACH_REQUEST_UNSCHEDULE,
	REACH_REQUEST_SNAPSHOT		= 0x0101,
};

#define	REACH_TARGET_NAME		"name"			// string
#define	REACH_TARGET_SERV		"serv"			// string
#define	REACH_TARGET_HINTS		"hints"			// data (struct addrinfo)
#define	REACH_TARGET_IF_INDEX		"if_index"		// int64
#define	REACH_TARGET_IF_NAME		"if_name"		// string
#define	REACH_TARGET_LOCAL_ADDR		"localAddress"		// data (struct sockaddr)
#define	REACH_TARGET_REMOTE_ADDR	"remoteAddress"		// data (struct sockaddr)
#define	REACH_TARGET_ONDEMAND_BYPASS	"onDemandBypass"	// bool
#define REACH_TARGET_RESOLVER_BYPASS	"resolverBypass"	// bool


#define REACH_REQUEST_REPLY		"reply"			// int64
#define REACH_REQUEST_REPLY_DETAIL	"reply_detail"		// string

enum {
	REACH_REQUEST_REPLY_OK		= 0x0000,
	REACH_REQUEST_REPLY_FAILED,
	REACH_REQUEST_REPLY_UNKNOWN,
};


// ------------------------------------------------------------


#pragma mark -
#pragma mark [XPC] Reachability Server (server->client request)


#define	MESSAGE_NOTIFY			"notify_op"		// int64

enum {
	MESSAGE_REACHABILITY_STATUS	= 0x1001,
};

#define REACH_STATUS_CYCLE			"cycle"			// uint64
#define REACH_STATUS_FLAGS			"flags"			// uint64
#define REACH_STATUS_IF_INDEX			"if_index"		// uint64
#define REACH_STATUS_IF_NAME			"if_name"		// data (char if_name[IFNAMSIZ])
#define REACH_STATUS_RESOLVED_ADDRESS		"resolvedAddress"	// array[data]
#define REACH_STATUS_RESOLVED_ADDRESS_ERROR	"resolvedAddressError"	// int64
#define REACH_STATUS_SLEEPING			"sleeping"		// bool


// ------------------------------------------------------------

#endif	// HAVE_REACHABILITY_SERVER


__BEGIN_DECLS

CFStringRef
_SCNetworkReachabilityCopyTargetDescription	(SCNetworkReachabilityRef	target);

CFStringRef
_SCNetworkReachabilityCopyTargetFlags		(SCNetworkReachabilityRef	target);

#ifdef	HAVE_REACHABILITY_SERVER

dispatch_queue_t
__SCNetworkReachability_concurrent_queue	(void);

void
__SCNetworkReachabilityPerformNoLock		(SCNetworkReachabilityRef	target);

#pragma mark -
#pragma mark [XPC] Reachability Server (client APIs)

Boolean
_SCNetworkReachabilityServer_snapshot		(void);

Boolean
__SCNetworkReachabilityServer_targetAdd		(SCNetworkReachabilityRef	target);

void
__SCNetworkReachabilityServer_targetRemove	(SCNetworkReachabilityRef	target);

Boolean
__SCNetworkReachabilityServer_targetSchedule	(SCNetworkReachabilityRef	target);

Boolean
__SCNetworkReachabilityServer_targetStatus	(SCNetworkReachabilityRef	target);

Boolean
__SCNetworkReachabilityServer_targetUnschedule	(SCNetworkReachabilityRef	target);

Boolean
__SC_checkResolverReachabilityInternal		(SCDynamicStoreRef		*storeP,
						 SCNetworkReachabilityFlags	*flags,
						 Boolean			*haveDNS,
						 const char			*nodename,
						 const char			*servname,
						 uint32_t			*resolver_if_index,
						 int				*dns_config_index);

#endif	// HAVE_REACHABILITY_SERVER

__END_DECLS

#endif // _SCNETWORKREACHABILITYINTERNAL_H
