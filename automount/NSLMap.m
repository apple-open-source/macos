/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#import "NSLMap.h"
#import "Controller.h"
#import "NSLVnode.h"
#import "AMString.h"
#import "automount.h"
#import "log.h"
#import <stdio.h>
#import <string.h>
#import <unistd.h>
#import <syslog.h>
#import <arpa/inet.h>	/* for inet_ntoa */
#import <DNSServiceDiscovery/DNSServiceDiscovery.h>

#define RESOLVENEWBROWSERREPLIES 0

void
MyHandleMachMessage ( CFMachPortRef port, void * msg, CFIndex size, void * info )
{
    DNSServiceDiscovery_handleReply(msg);
}

void ResolveReply (
                    struct sockaddr 	*interface,
                    struct sockaddr 	*address,
                    char 		*txtRecord,
                    int 		flags,
                    void		*context
                    )
{
    sys_msg(debug, LOG_DEBUG, "ResolveReply: address port = %d, family = %d, address = %s",
                                ((struct sockaddr_in *)address)->sin_port,
                                ((struct sockaddr_in *)address)->sin_family,
                                inet_ntoa(((struct in_addr)((struct sockaddr_in *)address)->sin_addr)));
    return;
}


void DNSBrowseReply (
                   int 	resultType,		// One of DNSServiceBrowserReplyResultType
                   char  	*replyName,
                   char  	*replyType,
                   char  	*replyDomain,
                   int 	flags,			// DNS Service Discovery reply flags information
                   void	*context
                   )
{
    NSLMap *map = (NSLMap *)context;
    
    if (resultType == DNSServiceBrowserReplyAddInstance) {
#if RESOLVENEWBROWSERREPLIES
        // we have a find, let's resolve it
        CFMachPortRef           cfMachPort;
        CFMachPortContext       context;
        Boolean                 shouldFreeInfo;
        dns_service_discovery_ref 	dns_client;
        mach_port_t			port = NULL;
        CFRunLoopSourceRef		rls;
#endif
        sys_msg(debug, LOG_DEBUG, "DNSBrowseReply: Someone showed up with the name = %s, domain = %s, type = %s", replyName, replyDomain, replyType);
    
#if RESOLVENEWBROWSERREPLIES
        context.version                 = 1;
        context.info                    = 0;
        context.retain                  = NULL;
        context.release                 = NULL;
        context.copyDescription 	    = NULL;
    
        // start an enumerator on the local server
        dns_client = DNSServiceResolverResolve
            (
            replyName,
            replyType,
            replyDomain,
            ResolveReply,
            nil
            );
		
        if (dns_client) port = DNSServiceDiscoveryMachPort(dns_client);
    
        if (port) {
            cfMachPort = CFMachPortCreateWithPort ( kCFAllocatorDefault, port, ( CFMachPortCallBack ) MyHandleMachMessage,&context,&shouldFreeInfo );
    
            /* Create and add a run loop source for the port */
            rls = CFMachPortCreateRunLoopSource(NULL, cfMachPort, 0);
            CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
            CFRelease(rls);
        } else {
            sys_msg(debug, LOG_DEBUG, " DNSBrowseReply: Could not obtain client port");
            return;
        }
#endif
    } else if (resultType == DNSServiceBrowserReplyRemoveInstance) {
        sys_msg(debug, LOG_DEBUG, "DNSBrowseReply: Someone went away with the name = %s, domain = %s, type = %s", replyName, replyDomain, replyType);
    } else {
        sys_msg(debug, LOG_DEBUG, "DNSBrowseReply: unknown resultType (%d)?!", resultType);
    };

    [((NSLVnode *)[map root]) invalidateRecursively:YES];
    
    return;
}


void watch4mDNSNotifications(NSLMap *map)
{
    CFMachPortRef           cfMachPort;
    CFMachPortContext       context;
    Boolean                 shouldFreeInfo;
    mach_port_t			port;
    CFRunLoopSourceRef		rls;
    dns_service_discovery_ref 	browse_client;

    context.version                 = 1;
    context.info                    = 0;
    context.retain                  = NULL;
    context.release                 = NULL;
    context.copyDescription 	    = NULL;

    // start an enumerator on the local server
    browse_client = DNSServiceBrowserCreate
        (
        "_afpovertcp._tcp.",
        "local.arpa",
        DNSBrowseReply,
        map
        );

    port = DNSServiceDiscoveryMachPort(browse_client);

    if (port) {
        cfMachPort = CFMachPortCreateWithPort ( kCFAllocatorDefault, port, ( CFMachPortCallBack ) MyHandleMachMessage,&context,&shouldFreeInfo );

        /* Create and add a run loop source for the port */
        rls = CFMachPortCreateRunLoopSource(NULL, cfMachPort, 0);
        CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
        CFRelease(rls);
    } else {
        sys_msg(debug, LOG_DEBUG, "watch4mDNSNotifications: Could not obtain client port");
        return;
    }
}

@implementation NSLMap

- (Map *)initWithParent:(Vnode *)p directory:(String *)dir from:(String *)ds
{
    OSStatus returnStatus;

	[super init];

	mountPoint = [controller mountDirectory];
	if (mountPoint != nil) [mountPoint retain];

	[self setName:ds];

    // Open NSL and get ourselves a client ref before attempting any NSL operations:
    returnStatus = NSLXOpenNavigationAPI( kNSLXDefaultProtocols, &clientRef );
	if (returnStatus) {
		sys_msg(debug, LOG_ERR, "Cannot open NSL navigation API (error = %ld)", returnStatus);
		return nil;
	};
    
	root = [[NSLVnode alloc] init];
	[root setMap:self];
	[root setName:dir];
	[root setMounted:NO];
	[root setServer:nil];
	[root setMode:00755];
	[(NSLVnode *)root setNSLObject:NSLMakeNewNeighborhood( "", NULL ) type:kNetworkNeighborhood];

	if (p != nil) [p addChild:root];
	[controller registerVnode:root];

	// create a pthread which can run the CFRunLoop and register for mDNS notifications
	watch4mDNSNotifications(self);

	return self;
}

- (void)timeout
{
	/* Do nothing */
}



- (void)dealloc {
    NSLCloseNavigationAPI( clientRef );
    
    [super dealloc];
}

- (NSLClientRef)getNSLClientRef
{
	return clientRef;
}

@end
