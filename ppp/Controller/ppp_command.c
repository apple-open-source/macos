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
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <syslog.h>
#include <CoreFoundation/CoreFoundation.h>

#ifdef	USE_SYSTEMCONFIGURATION_PUBLIC_APIS
#include <SystemConfiguration/SystemConfiguration.h>
#else	/* USE_SYSTEMCONFIGURATION_PUBLIC_APIS */
#include <SystemConfiguration/v1Compatibility.h>
#include <SystemConfiguration/SCSchemaDefinitions.h>
#endif	/* USE_SYSTEMCONFIGURATION_PUBLIC_APIS */

#include "ppp_msg.h"
#include "ppp_privmsg.h"
#include "../Family/ppp_defs.h"
#include "../Family/if_ppp.h"
#include "../Family/if_ppplink.h"

#include "ppp_client.h"
#include "ppp_command.h"
#include "ppp_manager.h"
#include "ppp_option.h"


/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Forward Declarations
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

extern TAILQ_HEAD(, ppp) 	ppp_head;
extern SCDSessionRef		gCfgCache;


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_status (struct client *client, struct msg *msg)
{
    struct ppp_status 		*stat = (struct ppp_status *)&msg->data[0];
    struct ifpppstatsreq 	rq;
    struct ppp			*ppp = ppp_findbyref(msg->hdr.m_link);
    struct timeval 		tval;
    struct timezone 		tzone;
    int		 		s;

    PRINTF(("PPP_STATUS\n"));

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        msg->hdr.m_len = 0;
        return 0;
    }

    bzero (stat, sizeof (struct ppp_status));
    switch (ppp->phase) {
        case PPP_STATERESERVED:
            stat->status = PPP_IDLE;		// Dial on demand waiting does not exist in the api
            break;
        case PPP_DISCONNECTLINK+1:
            stat->status = PPP_DISCONNECTLINK;	//PPP_HOLDOFF; this state doesn't exit in the API
            break;
        default:
            stat->status = ppp->phase;
    }

    if (stat->status == PPP_RUNNING) {

        s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0) {
            msg->hdr.m_result = errno;
            msg->hdr.m_len = 0;
            return 0;
        }

        bzero (&rq, sizeof (rq));

        sprintf(rq.ifr_name, "%s%d", ppp->name, ppp->ifunit);
        if (ioctl(s, SIOCGPPPSTATS, &rq) < 0) {
            close(s);
            msg->hdr.m_result = errno;
            msg->hdr.m_len = 0;
            return 0;
        }

        close(s);

        if (!gettimeofday(&tval, &tzone)) {
            if (ppp->conntime)
                stat->s.run.timeElapsed = tval.tv_sec - ppp->conntime;
            if (!ppp->maxtime)	// no limit...
                stat->s.run.timeRemaining = 0xFFFFFFFF;
            else
                stat->s.run.timeRemaining = ppp->maxtime - stat->s.run.timeElapsed;
        }

        stat->s.run.outBytes = rq.stats.p.ppp_obytes;
        stat->s.run.inBytes = rq.stats.p.ppp_ibytes;
        stat->s.run.inPackets = rq.stats.p.ppp_ipackets;
        stat->s.run.outPackets = rq.stats.p.ppp_opackets;
        stat->s.run.inErrors = rq.stats.p.ppp_ierrors;
        stat->s.run.outErrors = rq.stats.p.ppp_ierrors;
    }
    else {
        stat->s.disc.lastDiscCause = ppp->laststatus;
    }

    msg->hdr.m_result = 0;
    msg->hdr.m_len = sizeof(struct ppp_status);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_connect (struct client *client, struct msg *msg)
{
    struct options		*opts;
    struct ppp			*ppp = ppp_findbyref(msg->hdr.m_link);

    msg->hdr.m_len = 0;

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        return 0;
    }

    PRINTF(("PPP_CONNECT\n"));
        
    // first find current the appropriate set of options
    opts = client_findoptset(client, msg->hdr.m_link);

    msg->hdr.m_result = ppp_doconnect(ppp, opts, 0);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_disconnect (struct client *client, struct msg *msg)
{
    struct ppp		*ppp = ppp_findbyref(msg->hdr.m_link);
    
    PRINTF(("PPP_DISCONNECT\n"));

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        msg->hdr.m_len = 0;
        return 0;
    }
    
    ppp_dodisconnect(ppp, 15); /* 1 */

    msg->hdr.m_result = 0;
    msg->hdr.m_len = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_enable_event (struct client *client, struct msg *msg)
{
    PRINTF(("PPP_ENABLE_EVENT\n"));

    client->notify = 1;
    client->notify_link = msg->hdr.m_link;
    msg->hdr.m_result = 0;
    msg->hdr.m_len = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_disable_event (struct client *client, struct msg *msg)
{
    PRINTF(("PPP_DISABLE_EVENT\n"));

    client->notify = 0;    
    msg->hdr.m_result = 0;
    msg->hdr.m_len = 0;
    return 0;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_version (struct client *client, struct msg *msg)
{
    PRINTF(("PPP_DISABLE_EVENT\n"));

    msg->hdr.m_result = 0;
    msg->hdr.m_len = sizeof(u_int32_t);
    *(u_int32_t*)&msg->data[0] = CURRENT_VERSION;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_getnblinks (struct client *client, struct msg *msg)
{
    u_long		nb = 0;
    struct ppp		*ppp;
    u_short		subtype = msg->hdr.m_link >> 16;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if ((subtype == 0xFFFF)
            || ( subtype == ppp->subtype)) {
            nb++;
        }
    }

    *(u_long *)&msg->data[0] = nb;

    msg->hdr.m_result = 0;
    msg->hdr.m_len = 4;
    return 0;

}

/* -----------------------------------------------------------------------------
index is a global index across all the link types (or within the family)
index if between 0 and nblinks
----------------------------------------------------------------------------- */
u_long ppp_getlinkbyindex (struct client *client, struct msg *msg)
{
    u_long		nb = 0, len = 0, err = ENODEV, index;
    struct ppp		*ppp;
    u_short		subtype = msg->hdr.m_link >> 16;

    index = *(u_long *)&msg->data[0];

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if ((subtype == 0xFFFF)
            || (subtype == ppp->subtype)) {
            if (nb == index) {
                *(u_long *)&msg->data[0] = ppp_makeref(ppp);
                err = 0;
                len = 4;
                break;
            }
            nb++;
        }
    }

    msg->hdr.m_result = err;
    msg->hdr.m_len = len;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_getlinkbyserviceid (struct client *client, struct msg *msg)
{
    u_long		len = 0, err = ENODEV;
    struct ppp		*ppp;
    CFStringRef		ref;

    msg->data[msg->hdr.m_len] = 0;
    ref = CFStringCreateWithCString(NULL, msg->data, kCFStringEncodingUTF8);
    if (ref) {
	ppp = ppp_findbyserviceID(ref);
        if (ppp) {
            *(u_long *)&msg->data[0] = ppp_makeref(ppp);
            err = 0;
            len = 4;
        }
        CFRelease(ref);
    }
    else 
        err = ENOMEM;
    
    msg->hdr.m_result = err;
    msg->hdr.m_len = len;
    return 0;
}

