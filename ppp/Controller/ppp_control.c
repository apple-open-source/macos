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

#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <CoreFoundation/CoreFoundation.h>

#include "ppp_msg.h"
#include "../Family/PPP.kmodproj/ppp.h"

#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "chap.h"
#include "upap.h"
#include "auth.h"
#include "magic.h"
#include "ppp_client.h"
#include "ppp_option.h"
#include "ppp_utils.h"
#include "ppp_command.h"
#include "ppp_manager.h"
#include "ppp_utils.h"
#include "link.h"

/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Forward Declarations
----------------------------------------------------------------------------- */

u_long getnblinks (u_short subfam);
int getlinkbyindex (u_short subfam, u_long index, u_long *link);
int setnblinks (u_short subfam, u_short nb);
int getlinkcaps(u_long link, struct ppp_caps *caps);

/* -----------------------------------------------------------------------------
Forward Declarations
----------------------------------------------------------------------------- */

extern TAILQ_HEAD(, ppp) 	ppp_head;


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_control(u_short id, struct msg *msg)
{
    struct ppp_ctl 		*ctl = (struct ppp_ctl *)&msg->data[0];
    u_long 			err = 0;
    
    switch (ctl->c_type) {

        case PPP_CTL_GETNBLINKS:
            msg->hdr.m_len = 4;
            *(u_long *)&ctl->c_data[0] = getnblinks (*(u_long *)&ctl->c_data[0]);
            break;

        case PPP_CTL_GETLINKBYINDEX:
            msg->hdr.m_len = 4;
            if (err = getlinkbyindex(*(u_long *)&ctl->c_data[0], *(u_long *)&ctl->c_data[4], (u_long *)&ctl->c_data[0]))
                msg->hdr.m_len = 0;
           break;

        case PPP_CTL_SETNBLINKS:
            msg->hdr.m_len = 0;
            err = setnblinks(*(u_long *)&ctl->c_data[0], *(u_long *)&ctl->c_data[4]);
            break;

        case PPP_CTL_GETLINKCAPS:
            msg->hdr.m_len = sizeof(struct ppp_caps);
            if (err = getlinkcaps(msg->hdr.m_link, (struct ppp_caps *)&ctl->c_data[0])) 
                msg->hdr.m_len = 0;            
            break;

        default:
            err = EOPNOTSUPP;
            msg->hdr.m_len = 0;
            return 0;
    }

    msg->hdr.m_result = err;
    msg->hdr.m_len += sizeof(struct ppp_ctl_hdr);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long getnblinks (u_short subfam)
{
    u_long		nb = 0;
    struct ppp		*ppp;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if ((subfam == 0xFFFF)
            || ( subfam == ppp->subfamily)) {
            nb++;
        }
    }

    return nb;
}

/* -----------------------------------------------------------------------------
index is a global index across all the link types
index if between 0 and nblinks
----------------------------------------------------------------------------- */
int getlinkbyindex (u_short subfam, u_long index, u_long *link)
{
    u_long		nb = 0;
    struct ppp		*ppp;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if ((subfam == 0xFFFF)
            || (subfam == ppp->subfamily)) {
            if (nb == index) {
                *link = ppp_makeref(ppp);
                return 0;
            }
            nb++;
        }
    }

    return ENODEV;
}

/* -----------------------------------------------------------------------------
at least 1 link of this type must exist
----------------------------------------------------------------------------- */
int setnblinks (u_short subfam, u_short nb)
{
    struct ppp		*ppp;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if ((subfam == 0xFFFF)
            || (subfam == ppp->subfamily)) {
            
            return link_setnblinks(ppp, nb) ;
        }
    }

   return ENODEV;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int getlinkcaps(u_long link, struct ppp_caps *caps)
{
    struct ppp		*ppp = ppp_findbyref(link);

    if (!ppp) 
        return ENODEV;

   // humm.. change it with portable data struture
    bcopy(&ppp->link_caps, caps, sizeof(struct ppp_caps));

    return 0;
}
