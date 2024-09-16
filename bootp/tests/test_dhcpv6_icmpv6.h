/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#ifndef test_dhcpv6_icmpv6_h
#define test_dhcpv6_icmpv6_h

#include <sys/types.h>
#include <sys/socket.h>
#import <netinet/in.h>
#import <netinet6/in6.h>
#import <netinet6/in6_var.h>
#import <netinet/icmp6.h>
#import <netinet/ip6.h>

#import "DHCPv6.h"
#import "DHCPv6Options.h"
#import "interfaces.h"
#import "IPv6Socket.h"

#import "test_utils.h"

#define DHCPV6_SERVER_QUEUE_LABEL "DHCPv6 Server Queue"
#define DHCPV6PD_SERVICE_QUEUE "DHCPv6 PD Service Queue"

#define ERR -1
#define NOERR 0
#define NONNULL_OR_ERROUT(ptr, str, ...) 	\
do {					\
if ((ptr) == NULL) {			\
NSLog(@str "\n", ## __VA_ARGS__);	\
return ERR;				\
}					\
} while (0)
#define NOERR_OR_ERROUT(err, str, ...) 		\
do {					\
if ((err) != 0) {			\
NSLog(@str "\n", ## __VA_ARGS__);	\
return ERR;				\
}					\
} while (0)

// fc00::10:0:10:1
#define DHCPV6_SERVER_ULA_IN6ADDR 				\
{{{ 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		\
0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x01 }}}

// fc00::10:0:11:1
#define DHCPV6_CLIENT_ULA_IN6ADDR 				\
{{{ 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		\
0x00, 0x10, 0x00, 0x00, 0x00, 0x11, 0x00, 0x01 }}}

#define FETH0_LINKADDR_BYTES { 0x66, 0x65, 0x74, 0x68, 0x00, 0x00 }
#define FETH0_LINKADDR_LEN 6
#define FETH1_LINKADDR_BYTES { 0x66, 0x65, 0x74, 0x68, 0x00, 0x01 }
#define FETH1_LINKADDR_LEN 6

typedef enum {
	kDHCPServerFailureModeNone = 0,
	kDHCPServerFailureModeNotOnLink = 1,
	kDHCPServerFailureModeNoPrefixAvail = 2
} DHCPServerFailureMode;

@interface DHCPv6Server : NSObject
@property int socket;
@property dispatch_source_t socketListener;
@property dispatch_queue_t queue;
@property int interfaceIndex;
@property NSData * duid;
@property BOOL clientConfigured;
@property DHCPServerFailureMode failureMode;
@property NSDate * timeOfRequest;
@property NSTimeInterval timeBetweenSubsequentRequests1;
@property NSTimeInterval timeBetweenSubsequentRequests2;
@property dispatch_semaphore_t exponentialBackoffSem;

- (instancetype)initWithFailureMode:(DHCPServerFailureMode)failureMode
		       andInterface:(NSString *)ifname;
- (void)disconnect;
@end

#if 0
@interface ICMPv6Router: NSObject
@property int socket;
@property dispatch_source_t socketListener;
@property dispatch_queue_t queue;
@property int interfaceIndex;
@property NSData * duid;
@property BOOL clientConfigured;
@end
#endif

#endif /* test_dhcpv6_icmpv6_h */
