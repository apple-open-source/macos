/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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


/* -----------------------------------------------------------------------------
includes
----------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/queue.h>
#import <CoreFoundation/CFSocket.h>

#include "ppp_msg.h"
#include "ppp_client.h"

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
struct client *client_new (CFSocketRef ref)
{
    struct client	*client;
    
    client = malloc(sizeof(struct client));
    if (!client)
        return 0;	// very bad...

    bzero(client, sizeof(struct client));

    CFRetain(ref);
    client->ref = ref;
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
    
    ppp_clientgone(client);

    while (opts = TAILQ_FIRST(&(client->opts_head))) {
        
        TAILQ_REMOVE(&(client->opts_head), opts, next);
        CFRelease(opts->serviceid);
        free(opts);
    }

    client->notify_link = 0;    
    client->notify_useserviceid = 0;    
    if (client->notify_serviceid) {
        free(client->notify_serviceid);
        client->notify_serviceid = 0;
    }
    
    if (client->msg) {
        CFAllocatorDeallocate(NULL, client->msg);
        client->msg = 0;
    }
    client->msglen = 0;
    client->msgtotallen = 0;
    CFRelease(client->ref);
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
u_long client_notify (u_char* sid, u_int32_t link, u_long event, u_long error, int notification)
{
    struct ppp_msg_hdr	msg;
    struct client	*client;
    int 		doit;
    
    TAILQ_FOREACH(client, &client_head, next) {
        doit = 0;
        
        if (client->notify & notification) {
            if (client->notify_useserviceid) {
                doit = ((client->notify_serviceid == 0)	// any service
                        || !strcmp(client->notify_serviceid, sid));
            }
            else { 
                doit = ((client->notify_link == link)			// exact same link
                        || ((client->notify_link >> 16) == 0xFFFF)	// all kind of links
                        || (((client->notify_link >> 16) == (link >> 16))// all links of that kind
                            && ((client->notify_link >> 16) == 0xFFFF)));
            }
        }

        if (doit) {
            bzero(&msg, sizeof(msg));
            msg.m_type = PPP_EVENT;
            msg.m_link = link;
            msg.m_result = event;
            msg.m_cookie = error;
            if (client->notify_useserviceid) {
                msg.m_flags |= USE_SERVICEID;
                msg.m_link = strlen(sid);
            }
            
            if (writen(CFSocketGetNative(client->ref), &msg, sizeof(msg)) != sizeof(msg))
                continue;

            if (client->notify_useserviceid) {
                if (writen(CFSocketGetNative(client->ref), sid, msg.m_link) != msg.m_link)
                    continue;
            }
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
        if (client->ref == ref)
            return client;
            
    return 0;
}
