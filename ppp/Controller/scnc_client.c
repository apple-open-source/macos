/*
 * Copyright (c) 2000-2011 Apple Computer, Inc. All rights reserved.
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


/* -----------------------------------------------------------------------------
includes
----------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/queue.h>
#import <CoreFoundation/CFSocket.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <CoreFoundation/CFMachPort.h>
#include <CoreFoundation/CFBundle.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>

#include "ppp_msg.h"
#include "scnc_main.h"
#include "scnc_client.h"
#include "scnc_utils.h"
#include "scnc_mach_server.h"
#include "ppp_socket_server.h"


/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */


TAILQ_HEAD(, client) 	client_head;


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long client_init_all ()
{

    TAILQ_INIT(&client_head);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct client *client_new_socket (CFSocketRef ref, int priviledged, uid_t uid, gid_t gid)
{
    struct client	*client;
    
    client = malloc(sizeof(struct client));
    if (!client)
        return 0;	// very bad...

    bzero(client, sizeof(struct client));

    CFRetain(ref);
    client->socketRef = ref;
    if (priviledged)
		client->flags |= CLIENT_FLAG_PRIVILEDGED;
	client->uid = uid;
	client->gid = gid;
	client->flags |= CLIENT_FLAG_IS_SOCKET;
    TAILQ_INIT(&client->opts_head);

    TAILQ_INSERT_TAIL(&client_head, client, next);

    return client;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct client *client_new_mach (CFMachPortRef port, CFRunLoopSourceRef rls, CFStringRef serviceID, uid_t uid, gid_t gid, pid_t pid, mach_port_t bootstrap, mach_port_t notify_port, mach_port_t au_session)
{
    struct client	*client;
    
    client = malloc(sizeof(struct client));
    if (!client)
        return 0;	// very bad...

    bzero(client, sizeof(struct client));

    CFRetain(port);
    CFRetain(rls);
    CFRetain(serviceID);
    client->sessionPortRef = port;
    client->sessionRls = rls;
    client->serviceID = serviceID;
	client->uid = uid;
	client->gid = gid;
	client->pid = pid;
	client->bootstrap_port = bootstrap;
	client->notify_port = notify_port;
	client->flags &= ~CLIENT_FLAG_IS_SOCKET;
	client->au_session = au_session;
    TAILQ_INIT(&client->opts_head);

    TAILQ_INSERT_TAIL(&client_head, client, next);

    return client;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void client_dispose (struct client *client)
{
    struct client_opts	*opts;
    
    TAILQ_REMOVE(&client_head, client, next);


    while (opts = TAILQ_FIRST(&(client->opts_head))) {
        
        TAILQ_REMOVE(&(client->opts_head), opts, next);
        CFRelease(opts->serviceid);
        free(opts);
    }

    client->notify_link = 0;    
    if (client->notify_serviceid) {
        free(client->notify_serviceid);
        client->notify_serviceid = 0;
    }
    
    if (client->msg) {
        my_Deallocate(client->msg, client->msgtotallen + 1);
        client->msg = 0;
    }
    client->msglen = 0;
    client->msgtotallen = 0;
	
    if (client->bootstrap_port != MACH_PORT_NULL) {
	mach_port_deallocate(mach_task_self(), client->bootstrap_port);
	client->bootstrap_port = MACH_PORT_NULL;
    }

    if (client->notify_port != MACH_PORT_NULL) {
		mach_port_deallocate(mach_task_self(), client->notify_port);
		client->notify_port = MACH_PORT_NULL;
    }
    
    if (client->au_session != MACH_PORT_NULL) {
		mach_port_deallocate(mach_task_self(), client->au_session);
		client->au_session = MACH_PORT_NULL;
    }
    
	if (client->socketRef) {
		CFSocketInvalidate(client->socketRef);
		my_CFRelease(&client->socketRef);
	}
	
	if (client->sessionPortRef) {
		CFMachPortInvalidate(client->sessionPortRef);
		my_CFRelease(&client->sessionPortRef);
	}

	if (client->sessionRls) {
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), client->sessionRls, kCFRunLoopDefaultMode);
		my_CFRelease(&client->sessionRls);
	}

    my_CFRelease(&client->serviceID);
    free(client);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFMutableDictionaryRef client_newoptset (struct client *client, CFStringRef serviceid)
{
    struct client_opts	*opts;

    opts = malloc(sizeof(struct client_opts));
    if (!opts)
        return 0;	// very bad...

    bzero(opts, sizeof(struct client_opts));

    opts->opts = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (opts->opts == 0) {
        free(opts);
        return 0;	// very bad...
    }
        
    opts->serviceid = serviceid;
    CFRetain(opts->serviceid);
    TAILQ_INSERT_TAIL(&(client->opts_head), opts, next);

    return opts->opts;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFMutableDictionaryRef client_findoptset (struct client *client, CFStringRef serviceid)
{
    struct client_opts	*opts;
    
    TAILQ_FOREACH(opts, &(client->opts_head), next) 
        if (CFStringCompare(opts->serviceid, serviceid, 0) == kCFCompareEqualTo)
            return opts->opts;
    
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long client_notify (CFStringRef serviceID, u_char* sid, u_int32_t link, u_long event, u_long error, int notification, SCNetworkConnectionStatus status)
{
    struct client	*client;
    int 		doit;
    
    TAILQ_FOREACH(client, &client_head, next) {
        doit = 0;

        if (client->flags & notification) {
		
			if (client->flags & CLIENT_FLAG_IS_SOCKET) {
				if (client->notify_serviceid) {
					doit = ((client->notify_serviceid[0] == 0)	// any service
							|| !strcmp((char*)client->notify_serviceid, (char*)sid));
				}
				else { 
					doit = ((client->notify_link == link)			// exact same link
							|| ((client->notify_link >> 16) == 0xFFFF)	// all kind of links
							|| (((client->notify_link >> 16) == (link >> 16))// all links of that kind
								&& ((client->notify_link >> 16) == 0xFFFF)));
				}
			}
			else {
				doit = CFStringCompare(client->serviceID, serviceID, 0) == kCFCompareEqualTo;
			}
        }

        if (doit) {
			if (client->flags & CLIENT_FLAG_IS_SOCKET)
				socket_client_notify (client->socketRef, client->notify_serviceid ? sid : 0, link, event, error, client->flags);
			else 
				mach_client_notify (client->notify_port, client->serviceID, status, error);
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct client *client_findbysocketref(CFSocketRef ref)
{
    struct client	*client;

    TAILQ_FOREACH(client, &client_head, next)
        if (client->socketRef == ref)
            return client;
            
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct client *client_findbymachport(mach_port_t port)
{
    struct client	*client;

    TAILQ_FOREACH(client, &client_head, next)
        if (client->sessionPortRef
			&& port == CFMachPortGetPort(client->sessionPortRef))
            return client;
            
    return 0;
}


