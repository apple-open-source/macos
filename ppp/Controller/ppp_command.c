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


#include "ppp_msg.h"
#include "ppp_privmsg.h"
#include "../Family/PPP.kmodproj/ppp.h"

#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "chap.h"
#include "upap.h"
#include "auth.h"
#include "ppp_client.h"
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

extern TAILQ_HEAD(, ppp) 	ppp_head;


/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

extern CFSocketRef 		gEvtListenRef;


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_status (u_short id, struct msg *msg)
{
    struct ppp_status 		*stat = (struct ppp_status *)&msg->data[0];
    struct ifpppreq 		rq;
    struct ppp			*ppp = ppp_findbyref(msg->hdr.m_link);
    struct timeval 		tval;
    struct timezone 		tzone;

    PRINTF(("PPP_STATUS\n"));

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        msg->hdr.m_len = 0;
        return 0;
    }

    bzero (stat, sizeof (struct ppp_status));
    stat->status = ppp->phase;

    if (stat->status == PPP_RUNNING) {

        bzero (&rq, sizeof (rq));

        sprintf(rq.ifr_name, "%s%d", ppp->name, ppp->ifunit);
        rq.ifr_code = IFPPP_STATS;
        if  (ioctl(CFSocketGetNative(gEvtListenRef), SIOCGIFPPP, (caddr_t) &rq) < 0) {
            msg->hdr.m_result = errno;
            msg->hdr.m_len = 0;
            return 0;
        }


        if (!gettimeofday(&tval, &tzone)) {
            stat->s.run.timeElapsed = tval.tv_sec - ppp->conntime;
            if (!ppp->link_session_timer)	// no limit...
                stat->s.run.timeRemaining = 0xFFFFFFFF;
            else
                stat->s.run.timeRemaining = ppp->link_session_timer - stat->s.run.timeElapsed;
        }

        stat->s.run.outBytes = rq.ifr_stats.obytes;
        stat->s.run.inBytes = rq.ifr_stats.ibytes;
        stat->s.run.inPackets = rq.ifr_stats.ipackets;
        stat->s.run.outPackets = rq.ifr_stats.opackets;
        stat->s.run.inErrors = rq.ifr_stats.ierrors;
        stat->s.run.outErrors = rq.ifr_stats.ierrors;
    }
    else {
        stat->s.disc.lastDiscCause = ppp->status;
    }

    msg->hdr.m_result = 0;
    msg->hdr.m_len = sizeof(struct ppp_status);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_connect (u_short id, struct msg *msg)
{
    struct ppp		*ppp = ppp_findbyref(msg->hdr.m_link);
    struct options	*opts;
    int 		error;

    PRINTF(("PPP_CONNECT\n"));

    msg->hdr.m_result = 0;
    msg->hdr.m_len = 0;

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        return 0;
    }

    printf("ppp_connect, %s%d\n", ppp->name, ppp->unit);

    // can connect only in idle and in listen mode
    //if ((ppp->phase != PPP_IDLE) && (ppp->phase != PPP_LISTENING)) {
    if (ppp->phase != PPP_IDLE) {
        return 0;
    }

    ppp_new_phase(ppp, PPP_INITIALIZE);

    ppp->status = 0;
    ppp->lastmsg[0] = 0;
    ppp->connect_speed = 0;
    
    // first find current the appropriate set of options
    opts = client_findoptset(id, msg->hdr.m_link);
    if (!opts)
        opts = &ppp->def_options;

    //ppp_autoconnect_off(ppp);
    //ppp_reinit(ppp, 0);
    ppp_apply_options(ppp, opts, 0);

    if (ppp->ifunit == 0xFFFF) {
        ppp->need_connect = 1;
        link_attach(ppp);
    }
    else {
        error = link_connect(ppp, 0);
        if (error) {
            ppp_new_phase(ppp, PPP_IDLE);
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_apply (u_short id, struct msg *msg)
{
    struct ppp		*ppp = ppp_findbyref(msg->hdr.m_link);
    struct options	*opts;

    PRINTF(("PPP_APPLY\n"));

    msg->hdr.m_result = 0;
    msg->hdr.m_len = 0;

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        return 0;
    }

    printf("ppp_apply, %s%d\n", ppp->name, ppp->unit);

    // first make current the appropriate set of options
    opts = client_findoptset(id, msg->hdr.m_link);
    if (!opts)
        opts = &ppp->def_options;

    // then apply that set
    switch (ppp->phase) {
        case PPP_IDLE:
            if (ppp->link_state == link_listening)
                if (!link_abort(ppp))
                    ppp->link_ignore_disc = 1;

            ppp_reinit(ppp, 3);
            break;
        default:
            // apply options now, but most of them will effectively be used for the next connection
            ppp_apply_options(ppp, opts, 0);
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_listen (u_short id, struct msg *msg)
{
    int 		error;
    struct ppp		*ppp = ppp_findbyref(msg->hdr.m_link);
    struct options	*opts;

    //printf("ppp_listen, ppp%d\n", msg->hdr.m_link);
        
    msg->hdr.m_result = 0;
    msg->hdr.m_len = 0;

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        return 0;
    }

    if (ppp->phase != PPP_IDLE) {
        return 0;
    }

    ppp->status = 0;
    ppp->lastmsg[0] = 0;

    // first find the appropriate set of options
    opts = client_findoptset(id, msg->hdr.m_link);
    if (!opts)
        opts = &ppp->def_options;

    // then apply that set
    ppp_reinit(ppp, 0);
    ppp_apply_options(ppp, opts, 1);

    error = link_listen(ppp);
    if (error) {
        ppp_new_phase(ppp, PPP_IDLE);
        msg->hdr.m_result = error;
    }
    return 0;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_disconnect (u_short id, struct msg *msg)
{
    struct ppp		*ppp = ppp_findbyref(msg->hdr.m_link);
    struct options	*opts;

    PRINTF(("PPP_DISCONNECT\n"));

    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        msg->hdr.m_len = 0;
        return 0;
    }

    msg->hdr.m_result = 0;
    msg->hdr.m_len = 0;

    // first make current the appropriate set of options
    opts = client_findoptset(id, msg->hdr.m_link);
    if (!opts)
        opts = &ppp->def_options;

    switch (ppp->phase) {
        case PPP_IDLE:
            if (ppp->link_state != link_listening)
                break;
        //case PPP_LISTENING:
        case PPP_CONNECTLINK:
            link_abort(ppp);
            break;
        case PPP_INITIALIZE:
            ppp->need_connect = 0;
            ppp_new_phase(ppp, PPP_IDLE);
            break;
        default:
            link_disconnect(ppp);
            break;
    }

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_enable_event (u_short id, struct msg *msg)
{
    PRINTF(("PPP_ENABLE_EVENT\n"));

    clients[id]->notify = 1;
    clients[id]->notify_link = msg->hdr.m_link;
    msg->hdr.m_result = 0;
    msg->hdr.m_len = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_disable_event (u_short id, struct msg *msg)
{
    PRINTF(("PPP_DISABLE_EVENT\n"));

    clients[id]->notify = 0;    
    msg->hdr.m_result = 0;
    msg->hdr.m_len = 0;
    return 0;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_version (u_short id, struct msg *msg)
{
    PRINTF(("PPP_DISABLE_EVENT\n"));

    msg->hdr.m_result = 0;
    msg->hdr.m_len = sizeof(u_int32_t);
    *(u_int32_t*)&msg->data[0] = CURRENT_VERSION;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_openfd(u_short id, struct msg *msg)
{
    struct ppp	*ppp;
    u_long size = *(u_long *)&msg->data[0]; // the app will specify the size needed to be bufferized...

    printf("ppp_openfd, id = %d, fd = %d\n", id, CFSocketGetNative(clients[id]->ref));

    msg->hdr.m_result = 0;
    if (clients[id]->readfd_queue.data) {
        // already open
        msg->hdr.m_result = 1;  // ???
    }
    else {

        if (new_queue_data(&clients[id]->readfd_queue, size))
            msg->hdr.m_result = 1;  // ???
        else {
            // check for unknown link, find the first connecting serial
            if (msg->hdr.m_link == 0xFFFF) {
                TAILQ_FOREACH(ppp, &ppp_head, next) {
                    if ((ppp->subfamily == APPLE_IF_FAM_PPP_SERIAL)
                    && (ppp->phase == PPP_CONNECTLINK)) {
                        // return the actual link
                        msg->hdr.m_link = ppp->unit;
                        break;
                    }
                }
            }
            clients[id]->readfd_link = msg->hdr.m_link;
        }

    }

    msg->hdr.m_len = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_closeclientfd(u_long link)
{
    int 	i, fd;
    
    printf("ppp_closeclientfd\n");
    for (i = 0; i < MAX_CLIENT; i++) {
        if (!clients[i])
            continue;
        if (clients[i]->readfd_queue.data && (clients[i]->readfd_link == link)) {
            printf("ppp_closeclientfd, id = %d, fd = %d\n", i, CFSocketGetNative(clients[i]->ref));
// NO, need a better way to notify the client

            close_cleanup(i, 0);
            fd = DelSocketRef(clients[i]->ref);
            close(fd);
            client_dispose(i);
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_closefd(u_short id, struct msg *msg)
{

    printf("ppp_closefd, client id = %d\n", id);
    msg->hdr.m_result = 0;

    if (!clients[id]->readfd_queue.data) {
        // was not open
        msg->hdr.m_result = 1;  // ???
    }
    else {
        free_queue_data(&clients[id]->readfd_queue);
    }

    msg->hdr.m_len = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_writefd(u_short id, struct msg *msg)
{

    struct ppp		*ppp = ppp_findbyref(msg->hdr.m_link);

    if (ppp) {
        if (ppp->ttyref) {
             write(CFSocketGetNative(ppp->ttyref), &msg->data[0], msg->hdr.m_len);
            }
    }

    // may be should we return a status to tell write result ?
    msg->hdr.m_len = 0xFFFFFFFF; // no reply
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_readfd(u_short id, struct msg *msg)
{
    u_short 	len = *(u_long *)&msg->data[0];	// explicitly use short
    struct queue_data	*qd = &clients[id]->readfd_queue;

    if (!qd->data)
        return 0;

    if (len > qd->maxsize)
        len = qd->maxsize;

    if (qd->curlen) {
        dequeue_data(qd, &msg->data[0], &len);
        msg->hdr.m_len = len;
    }
    else {
        // should manage multipe pending read per client, on different link ???
        clients[id]->readfd_len = len;
        clients[id]->readfd_cookie = msg->hdr.m_cookie;
        //        clients[id]->readfd_link = msg->hdr.m_link;
        msg->hdr.m_len = 0xFFFFFFFF; // no reply
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void notifyreaders (u_long link, struct msg *msg)
{
    u_long 		i, pendinglen;
    struct queue_data	*qd;
    struct ppp_msg_hdr	rmsg;

    for (i = 0; i < MAX_CLIENT; i++) {
        if (!clients[i])
            continue;
        qd = &clients[i]->readfd_queue;
        if (qd->data && clients[i]->readfd_link == link) {
            if (msg->hdr.m_len <= clients[i]->readfd_len)
                clients[i]->readfd_len = msg->hdr.m_len;
            pendinglen = clients[i]->readfd_len;
            if (pendinglen) {
                clients[i]->readfd_len = 0;

                // notify client with the expected len or with what is just present
                bzero(&rmsg, sizeof(rmsg));
                rmsg.m_type = PPP_READFD;
                rmsg.m_cookie = clients[i]->readfd_cookie;
                rmsg.m_link = clients[i]->readfd_link;
                rmsg.m_len = pendinglen;

                //printf("notify reader, write1 nb = %d\n", sizeof(rmsg));
                if (write(CFSocketGetNative(clients[i]->ref), &rmsg, sizeof(rmsg)) != sizeof(rmsg)) {
                    //printf("notify reader, write1 errno = %d\n", errno);
                    continue;
                }

               //printf("notify reader, write2 nb = %d, c = 0x%x, '%c'\n", pendinglen, msg->data[0], msg->data[0] > ' ' ? msg->data[0] : ' ');
                if (write(CFSocketGetNative(clients[i]->ref), msg->data, pendinglen) != pendinglen) {
                    //printf("notify reader, write2 errno = %d\n", errno);
                    continue;
                }
                //printf("notify reader, done\n");
           }
            // enqueue the remainder
            enqueue_data(qd, &msg->data[pendinglen], msg->hdr.m_len - pendinglen);
        }
    }
}

/* -----------------------------------------------------------------------------
find a reader for this link and return the requested length
----------------------------------------------------------------------------- */
u_long findreader (u_long link)
{
    u_long 		i;
    struct queue_data	*qd;

    for (i = 0; i < MAX_CLIENT; i++) {
        if (!clients[i])
            continue;
        qd = &clients[i]->readfd_queue;
        if (qd->data && clients[i]->readfd_link == link && clients[i]->readfd_len) {
            return clients[i]->readfd_len;
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_getnblinks (u_short id, struct msg *msg)
{
    u_long		nb = 0;
    struct ppp		*ppp;
    u_short		subfam = msg->hdr.m_link >> 16;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if ((subfam == 0xFFFF)
            || ( subfam == ppp->subfamily)) {
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
u_long ppp_getlinkbyindex (u_short id, struct msg *msg)
{
    u_long		nb = 0, err = ENODEV, index;
    struct ppp		*ppp;
    u_short		subfam = msg->hdr.m_link >> 16;

    index = *(u_long *)&msg->data[0];

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if ((subfam == 0xFFFF)
            || (subfam == ppp->subfamily)) {
            if (nb == index) {
                *(u_long *)&msg->data[0] = ppp_makeref(ppp);
                err = 0;
                break;
            }
            nb++;
        }
    }

    msg->hdr.m_result = err;
    msg->hdr.m_len = 4;
    return 0;
}

#if 0
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
#endif
