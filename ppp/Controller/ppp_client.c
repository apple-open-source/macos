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

struct client 		*clients[MAX_CLIENT];



/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long client_init_all ()
{
    u_long	i;

    for (i = 0; i < MAX_CLIENT; i++)
        clients[i] = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long client_new (CFSocketRef ref)
{
    u_short		i;
    struct client	*client;
    
    for (i = 0; i < MAX_CLIENT; i++) {
        if (!clients[i]) 
            break;
    }

    if (i == MAX_CLIENT) {
        return 1;  // too many clients...
    }

    client = malloc(sizeof(struct client));
    if (!client)
        return 2;	// very bad...

    bzero(client, sizeof(struct client));

    TAILQ_INIT(&client->opts_head);

    clients[i] = client;
    client->ref = ref;

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long client_dispose (u_short id)
{

    struct client_opts	*opts;
    
    if (clients[id]) {

        while (opts = TAILQ_FIRST(&(clients[id]->opts_head))) {
            
            TAILQ_REMOVE(&(clients[id]->opts_head), opts, next);
            free(opts);
        }

        free(clients[id]);
        clients[id] = 0;
    }

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct options *client_newoptset (u_short id, u_long link)
{
    struct client_opts	*opts;

    opts = malloc(sizeof(struct client_opts));
    if (!opts)
        return 0;	// very bad...

    bzero(opts, sizeof(struct client_opts));

    TAILQ_INSERT_TAIL(&(clients[id]->opts_head), opts, next);

    opts->link = link;

    return &opts->opts;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct options *client_findoptset (u_short id, u_long link)
{
    struct client_opts	*opts;
    
    if (id < MAX_CLIENT) {
        TAILQ_FOREACH(opts, &(clients[id]->opts_head), next) {
            
            if (opts->link == link)
                return &opts->opts;
        }
    }
    
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long client_notify (u_long link, u_long state, u_long error)
{
    u_long		i;
    struct ppp_msg_hdr	msg;

    // broadcast the new state to all the listening socket
    for (i = 0; i < MAX_CLIENT; i++) {
        if (clients[i]
            && (clients[i]->notify == 1)
            && ((clients[i]->notify_link == link)			// exact same link
                || ((clients[i]->notify_link >> 16) == 0xFFFF)		// all kind of links
                || (((clients[i]->notify_link >> 16) == (link >> 16))	// all links of that kind
                    && ((clients[i]->notify_link >> 16) == 0xFFFF)))) {
            
            bzero(&msg, sizeof(msg));
            msg.m_type = PPP_EVENT;
            msg.m_link = link;
            msg.m_result = state;
            msg.m_cookie = error;

            if (write(CFSocketGetNative(clients[i]->ref), &msg, sizeof(msg)) != sizeof(msg))
                continue;
        }
    }

    return 0;
}
