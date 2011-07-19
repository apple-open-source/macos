/*
 * Copyright (c) 2000, 2001, 2003-2005, 2007-2011 Apple Inc. All rights reserved.
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

#include <netdb.h>
#include <netdb_async.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <dnsinfo.h>

static SCNetworkReachabilityRef
_setupReachability(int argc, char **argv, SCNetworkReachabilityContext *context)
{
	char				*ip_address	= argv[0];
	const char			*interface;
	struct sockaddr_in		sin;
	struct sockaddr_in6		sin6;
	SCNetworkReachabilityRef	target	= NULL;

	bzero(&sin, sizeof(sin));
	sin.sin_len    = sizeof(sin);
	sin.sin_family = AF_INET;

	bzero(&sin6, sizeof(sin6));
	sin6.sin6_len    = sizeof(sin6);
	sin6.sin6_family = AF_INET6;

	interface = strchr(argv[0], '%');
	if (interface != NULL) {
		ip_address = strdup(argv[0]);
		ip_address[interface - argv[0]] = '\0';
		interface++;
	}

	if (inet_aton(ip_address, &sin.sin_addr) == 1) {
		if (argc == 1) {
			if (interface == NULL) {
				target = SCNetworkReachabilityCreateWithAddress(NULL, (struct sockaddr *)&sin);
				if (context != NULL) {
					context->info = "by address";
				}
			} else {
				CFDataRef		data;
				CFStringRef		str;
				CFMutableDictionaryRef	options;

				if (if_nametoindex(interface) == 0) {
					SCPrint(TRUE, stderr, CFSTR("No interface: %s\n"), interface);
					exit(1);
				}

				options = CFDictionaryCreateMutable(NULL,
								    0,
								    &kCFTypeDictionaryKeyCallBacks,
								    &kCFTypeDictionaryValueCallBacks);
				data = CFDataCreate(NULL, (const UInt8 *)&sin, sizeof(sin));
				CFDictionarySetValue(options, kSCNetworkReachabilityOptionRemoteAddress, data);
				CFRelease(data);
				str  = CFStringCreateWithCString(NULL, interface, kCFStringEncodingASCII);
				CFDictionarySetValue(options, kSCNetworkReachabilityOptionInterface, str);
				CFRelease(str);
				target = SCNetworkReachabilityCreateWithOptions(NULL, options);
				if (context != NULL) {
					context->info = "by address w/scope";
				}
				CFRelease(options);
			}
		} else {
			char			*remote_address	= argv[1];
			const char		*interface2;
			struct sockaddr_in	r_sin;

			interface2 = strchr(argv[1], '%');
			if (interface2 != NULL) {
				remote_address = strdup(argv[1]);
				remote_address[interface2 - argv[1]] = '\0';
				interface2++;

				if ((interface != NULL) && (strcmp(interface, interface2) != 0)) {
					SCPrint(TRUE, stderr,
						CFSTR("Interface mismatch \"%s\" != \"%s\"\n"),
						interface,
						interface2);
					exit(1);
				}

				interface = interface2;
			}

			bzero(&r_sin, sizeof(r_sin));
			r_sin.sin_len    = sizeof(r_sin);
			r_sin.sin_family = AF_INET;
			if (inet_aton(remote_address, &r_sin.sin_addr) == 0) {
				SCPrint(TRUE, stderr, CFSTR("Could not interpret address \"%s\"\n"), remote_address);
				exit(1);
			}

			if (remote_address != argv[1]) {
				free(remote_address);
			}

			if (interface == NULL) {
				target = SCNetworkReachabilityCreateWithAddressPair(NULL,
										    (struct sockaddr *)&sin,
										    (struct sockaddr *)&r_sin);
				if (context != NULL) {
					context->info = "by address pair";
				}
			} else {
				CFDataRef		data;
				CFStringRef		str;
				CFMutableDictionaryRef	options;

				if (if_nametoindex(interface) == 0) {
					SCPrint(TRUE, stderr, CFSTR("No interface: %s\n"), interface);
					exit(1);
				}

				options = CFDictionaryCreateMutable(NULL,
								    0,
								    &kCFTypeDictionaryKeyCallBacks,
								    &kCFTypeDictionaryValueCallBacks);
				data = CFDataCreate(NULL, (const UInt8 *)&sin, sizeof(sin));
				CFDictionarySetValue(options, kSCNetworkReachabilityOptionLocalAddress, data);
				CFRelease(data);
				data = CFDataCreate(NULL, (const UInt8 *)&r_sin, sizeof(r_sin));
				CFDictionarySetValue(options, kSCNetworkReachabilityOptionRemoteAddress, data);
				CFRelease(data);
				str  = CFStringCreateWithCString(NULL, interface, kCFStringEncodingASCII);
				CFDictionarySetValue(options, kSCNetworkReachabilityOptionInterface, str);
				CFRelease(str);
				target = SCNetworkReachabilityCreateWithOptions(NULL, options);
				if (context != NULL) {
					context->info = "by address pair w/scope";
				}
				CFRelease(options);
			}
		}
	} else if (inet_pton(AF_INET6, argv[0], &sin6.sin6_addr) == 1) {
		if (interface != NULL) {
			sin6.sin6_scope_id = if_nametoindex(interface);
		}

		if (argc == 1) {
			target = SCNetworkReachabilityCreateWithAddress(NULL, (struct sockaddr *)&sin6);
			if (context != NULL) {
				context->info = "by (v6) address";
			}
		} else {
			struct sockaddr_in6	r_sin6;

			bzero(&r_sin6, sizeof(r_sin6));
			r_sin6.sin6_len         = sizeof(r_sin6);
			r_sin6.sin6_family      = AF_INET6;
			if (inet_pton(AF_INET6, argv[1], &r_sin6.sin6_addr) == 0) {
				SCPrint(TRUE, stderr, CFSTR("Could not interpret address \"%s\"\n"), argv[1]);
				exit(1);
			}

			interface = strchr(argv[1], '%');
			if (interface != NULL) {
				r_sin6.sin6_scope_id = if_nametoindex(interface);
			}

			target = SCNetworkReachabilityCreateWithAddressPair(NULL,
									    (struct sockaddr *)&sin6,
									    (struct sockaddr *)&r_sin6);
			if (context != NULL) {
				context->info = "by (v6) address pair";
			}
		}
	} else {
		if (argc == 1) {
			target = SCNetworkReachabilityCreateWithName(NULL, argv[0]);
			if (context != NULL) {
				context->info = "by name";
			}
		} else {
			CFStringRef		str;
			CFMutableDictionaryRef	options;

			options = CFDictionaryCreateMutable(NULL,
							    0,
							    &kCFTypeDictionaryKeyCallBacks,
							    &kCFTypeDictionaryValueCallBacks);
			if (strlen(argv[0]) > 0) {
				str  = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
				CFDictionarySetValue(options, kSCNetworkReachabilityOptionNodeName, str);
				CFRelease(str);
			}
			if (strlen(argv[1]) > 0) {
				str  = CFStringCreateWithCString(NULL, argv[1], kCFStringEncodingUTF8);
				CFDictionarySetValue(options, kSCNetworkReachabilityOptionServName, str);
				CFRelease(str);
			}
			if (argc > 2) {
				CFDataRef		data;
				struct addrinfo		hints	= { 0 };
				int			i;
				int			n_hints	= 0;

				for (i = 2; i < argc; i++) {
					if (strcasecmp(argv[i], "interface") == 0) {
						if (++i >= argc) {
							SCPrint(TRUE, stderr, CFSTR("No interface\n"));
							CFRelease(options);
							exit(1);
						}
						if (if_nametoindex(argv[i]) == 0) {
							SCPrint(TRUE, stderr, CFSTR("No interface: %s\n"), argv[i]);
							CFRelease(options);
							exit(1);
						}
						str  = CFStringCreateWithCString(NULL, argv[i], kCFStringEncodingASCII);
						CFDictionarySetValue(options, kSCNetworkReachabilityOptionInterface, str);
						CFRelease(str);
						continue;
					}

					if (strcasecmp(argv[i], "AI_ADDRCONFIG") == 0) {
						hints.ai_flags |= AI_ADDRCONFIG;
					} else if (strcasecmp(argv[i], "AI_ALL") == 0) {
						hints.ai_flags |= AI_ALL;
					} else if (strcasecmp(argv[i], "AI_V4MAPPED") == 0) {
						hints.ai_flags |= AI_V4MAPPED;
					} else if (strcasecmp(argv[i], "AI_V4MAPPED_CFG") == 0) {
						hints.ai_flags |= AI_V4MAPPED_CFG;
					} else if (strcasecmp(argv[i], "AI_ADDRCONFIG") == 0) {
						hints.ai_flags |= AI_ADDRCONFIG;
					} else if (strcasecmp(argv[i], "AI_V4MAPPED") == 0) {
						hints.ai_flags |= AI_V4MAPPED;
					} else if (strcasecmp(argv[i], "AI_DEFAULT") == 0) {
						hints.ai_flags |= AI_DEFAULT;
#ifdef	AI_PARALLEL
					} else if (strcasecmp(argv[i], "AI_PARALLEL") == 0) {
						hints.ai_flags |= AI_PARALLEL;
#endif	// AI_PARALLEL
					} else if (strcasecmp(argv[i], "PF_INET") == 0) {
						hints.ai_family = PF_INET;
					} else if (strcasecmp(argv[i], "PF_INET6") == 0) {
						hints.ai_family = PF_INET6;
					} else if (strcasecmp(argv[i], "SOCK_STREAM") == 0) {
						hints.ai_socktype = SOCK_STREAM;
					} else if (strcasecmp(argv[i], "SOCK_DGRAM") == 0) {
						hints.ai_socktype = SOCK_DGRAM;
					} else if (strcasecmp(argv[i], "SOCK_RAW") == 0) {
						hints.ai_socktype = SOCK_RAW;
					} else if (strcasecmp(argv[i], "IPPROTO_TCP") == 0) {
						hints.ai_protocol = IPPROTO_TCP;
					} else if (strcasecmp(argv[i], "IPPROTO_UDP") == 0) {
						hints.ai_protocol = IPPROTO_UDP;
					} else {
						SCPrint(TRUE, stderr, CFSTR("Unrecognized hint: %s\n"), argv[i]);
						CFRelease(options);
						exit(1);
					}
					n_hints++;
				}

				if (n_hints > 0) {
					data = CFDataCreate(NULL, (const UInt8 *)&hints, sizeof(hints));
					CFDictionarySetValue(options, kSCNetworkReachabilityOptionHints, data);
					CFRelease(data);
				}
			}
			if (CFDictionaryGetCount(options) > 0) {
				target = SCNetworkReachabilityCreateWithOptions(NULL, options);
				if (context != NULL) {
					context->info = "by (node and/or serv) name";
				}
			} else {
				SCPrint(TRUE, stderr, CFSTR("Must specify nodename or servname\n"));
				CFRelease(options);
				exit(1);
			}
			CFRelease(options);
		}
	}

	if (ip_address != argv[0]) {
		free(ip_address);
	}

	return target;
}

static void
_printReachability(SCNetworkReachabilityRef target)
{
	SCNetworkReachabilityFlags	flags;
	Boolean				ok;

	ok = SCNetworkReachabilityGetFlags(target, &flags);
	if (!ok) {
		printf("    could not determine reachability, %s\n", SCErrorString(SCError()));
		return;
	}

	SCPrint(_sc_debug, stdout, CFSTR("flags = 0x%08x"), flags);
	if (flags != 0) {
		SCPrint(_sc_debug, stdout, CFSTR(" ("));
		if (flags & kSCNetworkReachabilityFlagsReachable) {
			SCPrint(TRUE, stdout, CFSTR("Reachable"));
			flags &= ~kSCNetworkReachabilityFlagsReachable;
			SCPrint(flags != 0, stdout, CFSTR(","));
		}
		if (flags & kSCNetworkReachabilityFlagsTransientConnection) {
			SCPrint(TRUE, stdout, CFSTR("Transient Connection"));
			flags &= ~kSCNetworkReachabilityFlagsTransientConnection;
			SCPrint(flags != 0, stdout, CFSTR(","));
		}
		if (flags & kSCNetworkReachabilityFlagsConnectionRequired) {
			SCPrint(TRUE, stdout, CFSTR("Connection Required"));
			flags &= ~kSCNetworkReachabilityFlagsConnectionRequired;
			SCPrint(flags != 0, stdout, CFSTR(","));
		}
		if (flags & kSCNetworkReachabilityFlagsConnectionOnTraffic) {
			SCPrint(TRUE, stdout, CFSTR("Automatic Connection On Traffic"));
			flags &= ~kSCNetworkReachabilityFlagsConnectionOnTraffic;
			SCPrint(flags != 0, stdout, CFSTR(","));
		}
		if (flags & kSCNetworkReachabilityFlagsConnectionOnDemand) {
			SCPrint(TRUE, stdout, CFSTR("Automatic Connection On Demand"));
			flags &= ~kSCNetworkReachabilityFlagsConnectionOnDemand;
			SCPrint(flags != 0, stdout, CFSTR(","));
		}
		if (flags & kSCNetworkReachabilityFlagsInterventionRequired) {
			SCPrint(TRUE, stdout, CFSTR("Intervention Required"));
			flags &= ~kSCNetworkReachabilityFlagsInterventionRequired;
			SCPrint(flags != 0, stdout, CFSTR(","));
		}
		if (flags & kSCNetworkReachabilityFlagsIsLocalAddress) {
			SCPrint(TRUE, stdout, CFSTR("Local Address"));
			flags &= ~kSCNetworkReachabilityFlagsIsLocalAddress;
			SCPrint(flags != 0, stdout, CFSTR(","));
		}
		if (flags & kSCNetworkReachabilityFlagsIsDirect) {
			SCPrint(TRUE, stdout, CFSTR("Directly Reachable Address"));
			flags &= ~kSCNetworkReachabilityFlagsIsDirect;
			SCPrint(flags != 0, stdout, CFSTR(","));
		}
#if	TARGET_OS_IPHONE
		if (flags & kSCNetworkReachabilityFlagsIsWWAN) {
			SCPrint(TRUE, stdout, CFSTR("WWAN"));
			flags &= ~kSCNetworkReachabilityFlagsIsWWAN;
			SCPrint(flags != 0, stdout, CFSTR(","));
		}
#endif	// TARGET_OS_IPHONE
		if (flags != 0) {
			SCPrint(TRUE, stdout, CFSTR("0x%08x"), flags);
		}
		SCPrint(_sc_debug, stdout, CFSTR(")"));
	} else {
		SCPrint(_sc_debug, stdout, CFSTR(" ("));
		SCPrint(TRUE, stdout, CFSTR("Not Reachable"));
		SCPrint(_sc_debug, stdout, CFSTR(")"));
	}
	SCPrint(TRUE, stdout, CFSTR("\n"));

	return;
}


__private_extern__
void
do_checkReachability(int argc, char **argv)
{
	SCNetworkReachabilityRef	target;

	target = _setupReachability(argc, argv, NULL);
	if (target == NULL) {
		SCPrint(TRUE, stderr, CFSTR("  Could not determine status: %s\n"), SCErrorString(SCError()));
		exit(1);
	}

	_printReachability(target);
	CFRelease(target);
	exit(0);
}


static void
callout(SCNetworkReachabilityRef target, SCNetworkReachabilityFlags flags, void *info)
{
	static int	n = 3;
	struct tm	tm_now;
	struct timeval	tv_now;

	(void)gettimeofday(&tv_now, NULL);
	(void)localtime_r(&tv_now.tv_sec, &tm_now);

	SCPrint(TRUE, stdout, CFSTR("\n*** %2d:%02d:%02d.%03d\n\n"),
		tm_now.tm_hour,
		tm_now.tm_min,
		tm_now.tm_sec,
		tv_now.tv_usec / 1000);
	SCPrint(TRUE, stdout, CFSTR("%2d: callback w/flags=0x%08x (info=\"%s\")\n"), n++, flags, (char *)info);
	SCPrint(TRUE, stdout, CFSTR("    %@\n"), target);
	_printReachability(target);
	SCPrint(TRUE, stdout, CFSTR("\n"));
	return;
}


__private_extern__
void
do_watchReachability(int argc, char **argv)
{
	SCNetworkReachabilityContext	context	= { 0, NULL, NULL, NULL, NULL };
	SCNetworkReachabilityRef	target;
	SCNetworkReachabilityRef	target_async;

	target = _setupReachability(argc, argv, NULL);
	if (target == NULL) {
		SCPrint(TRUE, stderr, CFSTR("  Could not determine status: %s\n"), SCErrorString(SCError()));
		exit(1);
	}

	target_async = _setupReachability(argc, argv, &context);
	if (target_async == NULL) {
		SCPrint(TRUE, stderr, CFSTR("  Could not determine status: %s\n"), SCErrorString(SCError()));
		exit(1);
	}

	// Normally, we don't want to make any calls to SCNetworkReachabilityGetFlags()
	// until after the "target" has been scheduled on a run loop.  Otherwise, we'll
	// end up making a synchronous DNS request and that's not what we want.
	//
	// But, to test the case were an application call SCNetworkReachabilityGetFlags()
	// we provide the "CHECK_REACHABILITY_BEFORE_SCHEDULING" environment variable.
	if (getenv("CHECK_REACHABILITY_BEFORE_SCHEDULING") != NULL) {
		CFRelease(target_async);
		target_async = CFRetain(target);
	}

	// Direct check of reachability
	SCPrint(TRUE, stdout, CFSTR(" 0: direct\n"));
	SCPrint(TRUE, stdout, CFSTR("   %@\n"), target);
	_printReachability(target);
	CFRelease(target);
	SCPrint(TRUE, stdout, CFSTR("\n"));

	// schedule the target
	SCPrint(TRUE, stdout, CFSTR(" 1: start\n"));
	SCPrint(TRUE, stdout, CFSTR("   %@\n"), target_async);
	//_printReachability(target_async);
	SCPrint(TRUE, stdout, CFSTR("\n"));

	if (!SCNetworkReachabilitySetCallback(target_async, callout, &context)) {
		printf("SCNetworkReachabilitySetCallback() failed: %s\n", SCErrorString(SCError()));
		exit(1);
	}

	if (doDispatch) {
		if (!SCNetworkReachabilitySetDispatchQueue(target_async, dispatch_get_current_queue())) {
			printf("SCNetworkReachabilitySetDispatchQueue() failed: %s\n", SCErrorString(SCError()));
			exit(1);
		}
	} else {
		if (!SCNetworkReachabilityScheduleWithRunLoop(target_async, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode)) {
			printf("SCNetworkReachabilityScheduleWithRunLoop() failed: %s\n", SCErrorString(SCError()));
			exit(1);
		}
	}

	// Note: now that we are scheduled on a run loop we can call SCNetworkReachabilityGetFlags()
	//       to get the current status.  For "names", a DNS lookup has already been initiated.
	SCPrint(TRUE, stdout, CFSTR(" 2: on %s\n"), doDispatch ? "dispatch queue" : "runloop");
	SCPrint(TRUE, stdout, CFSTR("   %@\n"), target_async);
	_printReachability(target_async);
	SCPrint(TRUE, stdout, CFSTR("\n"));

	CFRunLoopRun();
	exit(0);
}


static void
showResolver(dns_resolver_t *resolver, int index)
{
	int	i;

	SCPrint(TRUE, stdout, CFSTR("\nresolver #%d\n"), index);

	if (resolver->domain != NULL) {
		SCPrint(TRUE, stdout, CFSTR("  domain   : %s\n"), resolver->domain);
	}

	for (i = 0; i < resolver->n_search; i++) {
		SCPrint(TRUE, stdout, CFSTR("  search domain[%d] : %s\n"), i, resolver->search[i]);
	}

	for (i = 0; i < resolver->n_nameserver; i++) {
		char	buf[128];

		_SC_sockaddr_to_string(resolver->nameserver[i], buf, sizeof(buf));
		SCPrint(TRUE, stdout, CFSTR("  nameserver[%d] : %s\n"), i, buf);
	}

	for (i = 0; i < resolver->n_sortaddr; i++) {
		char	abuf[32];
		char	mbuf[32];

		(void)inet_ntop(AF_INET, &resolver->sortaddr[i]->address, abuf, sizeof(abuf));
		(void)inet_ntop(AF_INET, &resolver->sortaddr[i]->mask,    mbuf, sizeof(mbuf));
		SCPrint(TRUE, stdout, CFSTR("  sortaddr[%d] : %s/%s\n"), i, abuf, mbuf);
	}

	if (resolver->options != NULL) {
		SCPrint(TRUE, stdout, CFSTR("  options  : %s\n"), resolver->options);
	}

	if (resolver->port != 0) {
		SCPrint(TRUE, stdout, CFSTR("  port     : %hd\n"), resolver->port);
	}

	if (resolver->timeout != 0) {
		SCPrint(TRUE, stdout, CFSTR("  timeout  : %d\n"), resolver->timeout);
	}

	if (resolver->if_index != 0) {
		char	buf[IFNAMSIZ];
		char	*if_name;

		if_name = if_indextoname(resolver->if_index, buf);
		SCPrint(TRUE, stdout, CFSTR("  if_index : %d (%s)\n"),
			resolver->if_index,
			(if_name != NULL) ? if_name : "?");
	}

	if (resolver->flags != 0) {
		uint32_t	flags	= resolver->flags;

		SCPrint(TRUE, stdout, CFSTR("  flags    : "));
		if (flags & DNS_RESOLVER_FLAGS_SCOPED) {
			SCPrint(TRUE, stdout, CFSTR("Scoped"));
			flags &= ~DNS_RESOLVER_FLAGS_SCOPED;
			SCPrint(flags != 0, stdout, CFSTR(","));
		}
		if (flags != 0) {
			SCPrint(TRUE, stdout, CFSTR("0x%08x"), flags);
		}
		SCPrint(TRUE, stdout, CFSTR("\n"));
	}

	if (resolver->search_order != 0) {
		SCPrint(TRUE, stdout, CFSTR("  order    : %d\n"), resolver->search_order);
	}

	return;
}


__private_extern__
void
do_showDNSConfiguration(int argc, char **argv)
{
	dns_config_t	*dns_config;

	dns_config = dns_configuration_copy();
	if (dns_config) {
		int	i;

		SCPrint(TRUE, stdout, CFSTR("DNS configuration\n"));

		for (i = 0; i < dns_config->n_resolver; i++) {
			dns_resolver_t	*resolver	= dns_config->resolver[i];

			showResolver(resolver, i + 1);
		}

		if ((dns_config->n_scoped_resolver > 0) && (dns_config->scoped_resolver != NULL)) {
			SCPrint(TRUE, stdout, CFSTR("\nDNS configuration (for scoped queries)\n"));

			for (i = 0; i < dns_config->n_scoped_resolver; i++) {
				dns_resolver_t	*resolver	= dns_config->scoped_resolver[i];

				showResolver(resolver, i + 1);
			}
		}

		dns_configuration_free(dns_config);
	} else {
		SCPrint(TRUE, stdout, CFSTR("No DNS configuration available\n"));
	}

	exit(0);
}


static void
showProxy(CFDictionaryRef proxy)
{
	CFMutableDictionaryRef	cleaned	= NULL;

	if (!_sc_debug) {
		cleaned = CFDictionaryCreateMutableCopy(NULL, 0, proxy);
		CFDictionaryRemoveValue(cleaned, kSCPropNetProxiesSupplemental);
		CFDictionaryRemoveValue(cleaned, kSCPropNetProxiesScoped);
		proxy = cleaned;
	}

	SCPrint(TRUE, stdout, CFSTR("%@\n"), proxy);
	if (cleaned != NULL) CFRelease(cleaned);
	return;
}


__private_extern__
void
do_showProxyConfiguration(int argc, char **argv)
{
	CFDictionaryRef proxies;

	proxies = SCDynamicStoreCopyProxies(NULL);
	if (proxies != NULL) {
		CFStringRef	interface	= NULL;
		CFStringRef	server		= NULL;

		while (argc > 0) {
			if (strcasecmp(argv[0], "interface") == 0) {
				argv++;
				argc--;

				if (argc < 1) {
					SCPrint(TRUE, stderr, CFSTR("No interface\n"));
					exit(1);
				}

				if (if_nametoindex(argv[0]) == 0) {
					SCPrint(TRUE, stderr, CFSTR("No interface: %s\n"), argv[0]);
					exit(1);
				}

				interface = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
				argv++;
				argc--;
			} else {
				if (server != NULL) {
					CFRelease(server);
				}

				server = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
				argv++;
				argc--;
			}
		}

		if ((server != NULL) || (interface != NULL)) {
			CFArrayRef	matching;

			matching = SCNetworkProxiesCopyMatching(proxies, server, interface);
			if (matching != NULL) {
				CFIndex	i;
				CFIndex	n;

				if (server != NULL) {
					if (interface != NULL) {
						SCPrint(TRUE, stdout,
							CFSTR("server = %@, interface = %@\n"),
							server,
							interface);
					} else {
						SCPrint(TRUE, stdout,
							CFSTR("server = %@\n"),
							server);
					}
				} else {
					SCPrint(TRUE, stdout,
						CFSTR("interface = %@\n"),
						interface);
				}

				n = CFArrayGetCount(matching);
				for (i = 0; i < n; i++) {
					CFDictionaryRef	proxy;

					proxy = CFArrayGetValueAtIndex(matching, i);
					SCPrint(TRUE, stdout, CFSTR("\nproxy #%d\n"), i + 1);
					showProxy(proxy);
				}

				CFRelease(matching);
			} else {
				SCPrint(TRUE, stdout, CFSTR("No matching proxy configurations\n"));
			}
		} else {
			showProxy(proxies);
		}

		if (interface != NULL) CFRelease(interface);
		if (server != NULL) CFRelease(server);
		CFRelease(proxies);
	} else {
		SCPrint(TRUE, stdout, CFSTR("No proxy configuration available\n"));
	}

	exit(0);
}


__private_extern__
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


__private_extern__
void
do_wait(char *waitKey, int timeout)
{
	struct itimerval	itv;
	CFStringRef		key;
	CFMutableArrayRef	keys;
	Boolean			ok;

	store = SCDynamicStoreCreate(NULL, CFSTR("scutil (wait)"), waitKeyFound, NULL);
	if (store == NULL) {
		SCPrint(TRUE, stderr,
			CFSTR("SCDynamicStoreCreate() failed: %s\n"), SCErrorString(SCError()));
		exit(1);
	}

	key  = CFStringCreateWithCString(NULL, waitKey, kCFStringEncodingUTF8);

	keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFArrayAppendValue(keys, key);
	ok = SCDynamicStoreSetNotificationKeys(store, keys, NULL);
	CFRelease(keys);
	if (!ok) {
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

	if (timeout > 0) {
		signal(SIGALRM, waitTimeout);
		bzero(&itv, sizeof(itv));
		itv.it_value.tv_sec = timeout;
		if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
			SCPrint(TRUE, stderr,
				CFSTR("setitimer() failed: %s\n"), strerror(errno));
			exit(1);
		}
	}

	CFRunLoopRun();
}

#ifdef	TEST_DNS_CONFIGURATION_COPY

CFRunLoopSourceRef	notifyRls	= NULL;
SCDynamicStoreRef	store		= NULL;
CFPropertyListRef	value		= NULL;

int
main(int argc, char **argv)
{
	do_showDNSConfiguration(argc, argv);
	exit(0);
}

#endif	// TEST_DNS_CONFIGURATION_COPY
