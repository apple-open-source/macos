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
#include <mach/mach_time.h>
#include <SystemConfiguration/SystemConfiguration.h>

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

u_int32_t ppp_translate_error(u_int16_t subtype, u_int32_t native_ppp_error, u_int32_t native_dev_error);

/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

extern TAILQ_HEAD(, ppp) 	ppp_head;
extern double	 		gTimeScaleSeconds;


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_status (struct client *client, struct msg *msg)
{
    struct ppp_status 		*stat = (struct ppp_status *)&msg->data[MSG_DATAOFF(msg)];
    struct ifpppstatsreq 	rq;
    struct ppp			*ppp = ppp_find(msg);
    int		 		s;
    u_int32_t			retrytime, curtime;

    PRINTF(("PPP_STATUS\n"));

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        msg->hdr.m_len = 0;
        return 0;
    }

    bzero (stat, sizeof (struct ppp_status));
    switch (ppp->phase) {
        case PPP_STATERESERVED:
        case PPP_HOLDOFF:
            stat->status = PPP_IDLE;		// Dial on demand does not exist in the api
            break;
        default:
            stat->status = ppp->phase;
    }

    switch (stat->status) {
        case PPP_RUNNING:
        case PPP_ONHOLD:

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

            curtime = mach_absolute_time() * gTimeScaleSeconds;
            if (ppp->conntime)
		stat->s.run.timeElapsed = curtime - ppp->conntime;
            if (!ppp->disconntime)	// no limit...
     	       stat->s.run.timeRemaining = 0xFFFFFFFF;
            else
      	      stat->s.run.timeRemaining = (ppp->disconntime > curtime) ? ppp->disconntime - curtime : 0;

            stat->s.run.outBytes = rq.stats.p.ppp_obytes;
            stat->s.run.inBytes = rq.stats.p.ppp_ibytes;
            stat->s.run.inPackets = rq.stats.p.ppp_ipackets;
            stat->s.run.outPackets = rq.stats.p.ppp_opackets;
            stat->s.run.inErrors = rq.stats.p.ppp_ierrors;
            stat->s.run.outErrors = rq.stats.p.ppp_ierrors;
            break;
            
        case PPP_WAITONBUSY:
        
            stat->s.busy.timeRemaining = 0;
            retrytime = 0;
            getNumberFromEntity(kSCDynamicStoreDomainState, ppp->serviceID, 
                kSCEntNetPPP, CFSTR("RetryConnectTime"), &retrytime);
            if (retrytime) {
                curtime = mach_absolute_time() * gTimeScaleSeconds;
                stat->s.busy.timeRemaining = (curtime < retrytime) ? retrytime - curtime : 0;
            }
            break;
         
        default:
            stat->s.disc.lastDiscCause = ppp_translate_error(ppp->subtype, ppp->laststatus, ppp->lastdevstatus);
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
    struct ppp			*ppp = ppp_find(msg);

    msg->hdr.m_len = 0;

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        return 0;
    }

    PRINTF(("PPP_CONNECT\n"));
        
    // first find current the appropriate set of options
    opts = client_findoptset(client, ppp_makeref(ppp));

    msg->hdr.m_result = ppp_doconnect(ppp, opts, 0);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_disconnect (struct client *client, struct msg *msg)
{
    struct ppp		*ppp = ppp_find(msg);
    
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
u_long ppp_suspend (struct client *client, struct msg *msg)
{
    struct ppp		*ppp = ppp_find(msg);
    
    PRINTF(("PPP_SUSPEND\n"));

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        msg->hdr.m_len = 0;
        return 0;
    }
    
    ppp_dosuspend(ppp); 

    msg->hdr.m_result = 0;
    msg->hdr.m_len = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_resume (struct client *client, struct msg *msg)
{
    struct ppp		*ppp = ppp_find(msg);
    
    PRINTF(("PPP_RESUME\n"));

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        msg->hdr.m_len = 0;
        return 0;
    }
    
    ppp_doresume(ppp);

    msg->hdr.m_result = 0;
    msg->hdr.m_len = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_enable_event (struct client *client, struct msg *msg)
{
    PRINTF(("PPP_ENABLE_EVENT\n"));

    msg->hdr.m_result = 0;
    client->notify = 1;
    client->notify_link = 0;
    client->notify_useservice = ((msg->hdr.m_flags & USE_SERVICEID) == USE_SERVICEID);
    if (client->notify_useservice && msg->hdr.m_link) {        
        if (client->notify_service = malloc(msg->hdr.m_link + 1)) {
            strncpy(client->notify_service, msg->data, msg->hdr.m_link);
            client->notify_service[msg->hdr.m_link] = 0;
        }
        else 
            msg->hdr.m_result = ENOMEM;
    }
    else 
        client->notify_link = msg->hdr.m_link;

    msg->hdr.m_len = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_disable_event (struct client *client, struct msg *msg)
{
    PRINTF(("PPP_DISABLE_EVENT\n"));

    client->notify = 0;    
    client->notify_link = 0;    
    client->notify_useservice = 0;    
    if (client->notify_service) {
    	free(client->notify_service);
        client->notify_service = 0;
    }
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

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_getlinkbyifname (struct client *client, struct msg *msg)
{
    u_long		len = 0, err = ENODEV;
    struct ppp		*ppp;
    u_char      	ifname[IFNAMSIZ] = { 0 };
    u_int16_t 		ifunit = 0, i = 0;

    while ((i < msg->hdr.m_len) && (i < sizeof(ifname)) 
        && ((msg->data[i] < '0') || (msg->data[i] > '9'))) {
        ifname[i] = msg->data[i];
        i++;
    }
    ifname[i] = 0;
    while ((i < msg->hdr.m_len) 
        && (msg->data[i] >= '0') && (msg->data[i] <= '9')) {
        ifunit = (ifunit * 10) + (msg->data[i] - '0');
        i++;
    }

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if (!strcmp(ppp->name, ifname)
            && (ppp->ifunit == ifunit)) {
            
            *(u_long *)&msg->data[0] = ppp_makeref(ppp);
            err = 0;
            len = 4;
            break;
        }
    }

    msg->hdr.m_result = err;
    msg->hdr.m_len = len;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_event(struct client *client, struct msg *msg)
{
    u_int32_t		event = *(u_int32_t *)&msg->data[0];
    u_int32_t		error = *(u_int32_t *)&msg->data[4];
    u_char 		*serviceid = &msg->data[8];
    CFStringRef		ref;
    struct ppp		*ppp;
    CFDictionaryRef	service = NULL;

    serviceid[msg->hdr.m_len - 8] = 0;	// need to zeroterminate the serviceid
    msg->hdr.m_len = 0xFFFFFFFF; // no reply
    //printf("ppp_event, event = 0x%x, cause = 0x%x, serviceid = '%s'\n", event, error, serviceid);

    ref = CFStringCreateWithCString(NULL, serviceid, kCFStringEncodingUTF8);
    if (ref) {
        ppp = ppp_findbyserviceID(ref);
        if (ppp) {
        
           // update status information first
            service = copyEntity(kSCDynamicStoreDomainState, ref, kSCEntNetPPP);
            if (service) {
                ppp_updatestate(ppp, service);
                CFRelease(service);
            }
        
            if (event == PPP_EVT_DISCONNECTED) {
                //if (error == EXIT_USER_REQUEST)
                //    return;	// PPP API generates PPP_EVT_DISCONNECTED only for unrequested disconnections
                error = ppp_translate_error(ppp->subtype, error, 0);
            }
            else 
                error = 0;
            client_notify(ppp->sid, ppp_makeref(ppp), event, error);
        }
        CFRelease(ref);
    }
}

/* -----------------------------------------------------------------------------
translate a pppd native cause into a PPP API cause
----------------------------------------------------------------------------- */
u_int32_t ppp_translate_error(u_int16_t subtype, u_int32_t native_ppp_error, u_int32_t native_dev_error)
{
    u_int32_t	error = PPP_ERR_GEN_ERROR; 
    
    switch (native_ppp_error) {
        case EXIT_USER_REQUEST:
            error = 0;
            break;
        case EXIT_CONNECT_FAILED:
            error = PPP_ERR_GEN_ERROR;
            break;
        case EXIT_TERMINAL_FAILED:
            error = PPP_ERR_TERMSCRIPTFAILED;
            break;
        case EXIT_NEGOTIATION_FAILED:
            error = PPP_ERR_LCPFAILED;
            break;
        case EXIT_AUTH_TOPEER_FAILED:
            error = PPP_ERR_AUTHFAILED;
            break;
        case EXIT_IDLE_TIMEOUT:
            error = PPP_ERR_IDLETIMEOUT;
            break;
        case EXIT_CONNECT_TIME:
            error = PPP_ERR_SESSIONTIMEOUT;
            break;
        case EXIT_LOOPBACK:
            error = PPP_ERR_LOOPBACK;
            break;
        case EXIT_PEER_DEAD:
            error = PPP_ERR_PEERDEAD;
            break;
        case EXIT_OK:
            error = PPP_ERR_DISCBYPEER;
            break;
        case EXIT_HANGUP:
            error = PPP_ERR_DISCBYDEVICE;
            break;
    }
    
    // override with a more specific error
    if (native_dev_error) {
        switch (subtype) {
            case PPP_TYPE_SERIAL:
                switch (native_dev_error) {
                    case EXIT_PPPSERIAL_NOCARRIER:
                        error = PPP_ERR_MOD_NOCARRIER;
                        break;
                    case EXIT_PPPSERIAL_NONUMBER:
                        error = PPP_ERR_MOD_NONUMBER;
                        break;
                    case EXIT_PPPSERIAL_BUSY:
                        error = PPP_ERR_MOD_BUSY;
                        break;
                    case EXIT_PPPSERIAL_NODIALTONE:
                        error = PPP_ERR_MOD_NODIALTONE;
                        break;
                    case EXIT_PPPSERIAL_ERROR:
                        error = PPP_ERR_MOD_ERROR;
                        break;
                    case EXIT_PPPSERIAL_NOANSWER:
                        error = PPP_ERR_MOD_NOANSWER;
                        break;
                    case EXIT_PPPSERIAL_HANGUP:
                        error = PPP_ERR_MOD_HANGUP;
                        break;
                    default :
                        error = PPP_ERR_CONNSCRIPTFAILED;
                }
                break;
    
            case PPP_TYPE_PPPoE:
                // need to handle PPPoE specific error codes
                break;
        }
    }
    
    return error;
}
