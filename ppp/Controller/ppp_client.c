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

    while (opts = TAILQ_FIRST(&(client->opts_head))) {
        
        TAILQ_REMOVE(&(client->opts_head), opts, next);
        free(opts);
    }

    CFRelease(client->ref);
    free(client);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct options *client_newoptset (struct client *client, u_long link)
{
    struct client_opts	*opts;

    opts = malloc(sizeof(struct client_opts));
    if (!opts)
        return 0;	// very bad...

    bzero(opts, sizeof(struct client_opts));

    opts->link = link;
    TAILQ_INSERT_TAIL(&(client->opts_head), opts, next);

    return &opts->opts;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct options *client_findoptset (struct client *client, u_long link)
{
    struct client_opts	*opts;
    
    TAILQ_FOREACH(opts, &(client->opts_head), next) 
        if (opts->link == link)
            return &opts->opts;
    
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long client_notify (u_char *serviceid, u_long link, u_long state, u_long error)
{
    struct ppp_msg_hdr	msg;
    struct client	*client;
    int 		doit;
    
    TAILQ_FOREACH(client, &client_head, next) {
        doit = 0;
        
        if (client->notify) {
            if (client->notify_useservice) {
                doit = (serviceid
                        && ((client->notify_service == 0)
                            || !strcmp(serviceid, client->notify_service)));
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
            msg.m_result = state;
            msg.m_cookie = error;
            if (client->notify_useservice) {
                msg.m_flags |= USE_SERVICEID;
                msg.m_link = strlen(serviceid);
            }
            
            if (write(CFSocketGetNative(client->ref), &msg, sizeof(msg)) != sizeof(msg))
                continue;

            if (client->notify_useservice) {
                if (write(CFSocketGetNative(client->ref), serviceid, msg.m_link) != msg.m_link)
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
