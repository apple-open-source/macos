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
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <net/dlil.h>
#include <sys/param.h>
#include <termios.h>
#include <fcntl.h>
#include <CoreFoundation/CoreFoundation.h>
#include <net/dlil.h>
#define	SYSTEMCONFIGURATION_NEW_API
#include <SystemConfiguration/SystemConfiguration.h>

#include "ppp_msg.h"
#include "ppp_privmsg.h"
#include "../Family/PPP.kmodproj/ppp.h"
#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "chap.h"
#include "upap.h"
#include "ppp_client.h"
#include "ppp_option.h"
#include "ppp_manager.h"
#include "ppp_command.h"
#include "ppp_utils.h"
#include "link.h"

/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */

enum {
    do_process = 0,
    do_close,
    do_error
};

/* -----------------------------------------------------------------------------
forward declarations
----------------------------------------------------------------------------- */


void initThings();
void postInitThings();
void startListen();
void socketCallBack(CFSocketRef s, CFSocketCallBackType type,
                     CFDataRef address, const void *data, void *info);
u_long processRequest (u_short id, struct msg *msg);

/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

struct msg 		gMsg;
CFSocketRef		gApiListenRef;

extern TAILQ_HEAD(, ppp) 	ppp_head;
extern CFSocketRef		gEvtListenRef;
extern CFSocketRef		gPPPProtoRef;


/* -----------------------------------------------------------------------------
plugin entry point, called by configd
----------------------------------------------------------------------------- */
void start(const char *bundleName, const char *bundleDir)
{
    initThings();
    startListen();
    postInitThings();
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void initThings()
{
    // load the PPP Family
    loadKext(KEXT_PPP);
}

/* -----------------------------------------------------------------------------
// init now things that need to have sockets instantiated
----------------------------------------------------------------------------- */
void postInitThings()
{

 //   CFUserNotificationDisplayNotice(30, kCFUserNotificationCautionAlertLevel, NULL, NULL, NULL, CFSTR("Christpohe Alerte"),
 //       CFSTR("just un petit coucou"), NULL);
    ppp_init_all();

}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void startListen ()
{
    struct sockaddr_un	serveraddr;
    int			error, s;
    mode_t		mask;

    printf("Running PPPController plugin...\n");

    s = socket(AF_LOCAL, SOCK_STREAM, 0);

    unlink(PPP_PATH);

    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sun_family = AF_LOCAL;
    strcpy(serveraddr.sun_path, PPP_PATH);
    mask = umask(0);
    error = bind(s, (struct sockaddr *)&serveraddr, SUN_LEN(&serveraddr));
    if (error) {
        printf("PPPController: Bind error... err = 0x%x\n", errno);
        umask(mask);
        return;
    }
    umask(mask);

    listen(s, SOMAXCONN);

    client_init_all();

    gApiListenRef = AddSocketNativeToRunLoop(s);

}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void socketCallback(CFSocketRef s, CFSocketCallBackType type,
                     CFDataRef address, const void *data, void *info)
{
    int 		fd = CFSocketGetNative(s);
    struct sockaddr_un	cliaddr;
    int			i, connfd, clilen, action;//, flags;
    ssize_t		n;
    struct ppp	 	*ppp;
    CFSocketRef		connRef;

    if (s == gApiListenRef) {
        clilen = sizeof(cliaddr);
        connfd = accept(fd, (struct sockaddr *) &cliaddr, &clilen);
        PRINTF(("Accepted connection...\n"));

 //       if ((flags = fcntl(connfd, F_GETFL)) == -1
 //           || fcntl(connfd, F_SETFL, flags | O_NONBLOCK) == -1) {
 //           printf("Couldn't set accepting socket in non-blocking mode, errno = %d\n", errno);
 //       }

        connRef = AddSocketNativeToRunLoop(connfd);

        if (client_new(connRef)) {
            DelSocketRef(connRef);
            close(connfd);
            return;			// "too many clients", Fix Me
        }
        return;
    }

    for (i = 0; i < MAX_CLIENT; i++) {	// check all clients for data
        if (!(clients[i] && (s == clients[i]->ref)))
            continue;

        action = do_process;
        if ((n = read(fd, &gMsg, sizeof(struct ppp_msg_hdr))) == 0) {
            action = do_close;
        } else {
            PRINTF(("Data to process... len = %d\n", gmsg.hdr.m_len));

            if (gMsg.hdr.m_len) {
                if (gMsg.hdr.m_len > MAXDATASIZE) {
                    action = do_error;
                }
                else if (read(fd, &gMsg.data[0], gMsg.hdr.m_len) != gMsg.hdr.m_len) {
                    action = do_close;
                }
            }
        }

        switch (action) {
            case do_close:
                PRINTF(("Connection closed...\n"));
                /* connection closed by client */
                close_cleanup(i, &gMsg);
                DelSocketRef(s);
                client_dispose(i);
                break;
            case do_error:
                PRINTF(("Connection length error...\n"));
                break;
            case do_process:
                // process client request
                processRequest(i, &gMsg);
                break;
        }
        break; // done with this request
   }

    if (i >= MAX_CLIENT) {
        // do it better...
        if (s == gEvtListenRef) {
            link_event();
        }

        if (s == gPPPProtoRef) {

            action = do_process;
           if ((n = read(fd, &gMsg.data[0], MAXDATASIZE)) == 0) {
                action = do_close;
            }

            switch (action) {
                case do_close:
                    PRINTF(("Connection closed...\n"));
                    /* connection closed by client */
                    // do linkdown here
                    break;
                case do_process:
                    gMsg.hdr.m_len = n;
                    ppp_readsockfd_data(&gMsg);
                    break;
            }
        }

        // reloop around tty fd...
        // Fix this algo ASAP !
        
        TAILQ_FOREACH(ppp, &ppp_head, next) {

            if (s == ppp->ttyref) {

                n = findreader(ppp_makeref(ppp));
                if (n == 0)
                    break;	// no pending read
                
               action = do_process;
                if ((n = read(fd, &gMsg.data[0], n /*MAXDATASIZE*/)) == 0) {
                    action = do_close;
                }
                switch (action) {
                    case do_close:
                        PRINTF(("Connection closed...\n"));
                        /* connection closed by client */
                        // do linkdown here
                        break;
                    case do_process:
                        gMsg.hdr.m_len = n;
                        ppp_readfd_data(ppp_makeref(ppp), &gMsg);
                        break;
                }
            }
        }
    }

    return;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long processRequest (u_short id, struct msg *msg)
{
    struct ppp_status *stat;

    PRINTF(("process_request : type = %x, len = %d\n", msg->hdr.m_type, msg->hdr.m_len));

    switch (msg->hdr.m_type) {
        case PPP_VERSION:
            ppp_version(id, msg);
            break;
        case PPP_STATUS:
            ppp_status(id, msg);
            break;
        case PPP_GETOPTION:
            ppp_getoption(id, msg);
            break;
        case PPP_SETOPTION:
            ppp_setoption(id, msg);
            break;
        case PPP_CONNECT:
            ppp_connect(id, msg);
            break;
        case PPP_DISCONNECT:
            ppp_disconnect(id, msg);
            break;
        case PPP_LISTEN:
            ppp_listen(id, msg);
            break;
        case PPP_ENABLE_EVENT:
            ppp_enable_event(id, msg);
            break;
        case PPP_DISABLE_EVENT:
            ppp_disable_event(id, msg);
            break;
        case PPP_APPLY:
            ppp_apply(id, msg);
            break;
        case PPP_GETNBLINKS:
            ppp_getnblinks(id, msg);
            break;
        case PPP_GETLINKBYINDEX:
            ppp_getlinkbyindex(id, msg);
            break;

            // ppp extensions calls
        case PPP_CCLNOTE:
            ppp_cclnote(id, msg);
            break;
        case PPP_CCLSPEED:
            ppp_cclspeed(id, msg);
            break;
        case PPP_CCLRESULT:
            ppp_cclresult(id, msg);
            break;
        case PPP_OPENFD:
            ppp_openfd(id, msg);
            break;
        case PPP_CLOSEFD:
            ppp_closefd(id, msg);
            break;
        case PPP_WRITEFD:
            ppp_writefd(id, msg);
            break;
        case PPP_READFD:
            ppp_readfd(id, msg);
            break;
        case PPP_CCLWRITETEXT:
            ppp_cclwritetext(id, msg);
            break;
        case PPP_CCLMATCHTEXT:
            ppp_cclmatchtext(id, msg);
            break;
    }

    if (msg->hdr.m_len != 0xFFFFFFFF) {

        write(CFSocketGetNative(clients[id]->ref), msg, sizeof(struct ppp_msg_hdr) + msg->hdr.m_len);
        //PRINTF(("process_request : write on output, len = %d\n", sizeof(struct ppp_msg_hdr) + msg->hdr.m_len));
        PRINTF(("process_request : m_type = 0x%x, result = 0x%x, cookie = 0x%x, link = 0x%x, len = 0x%x\n",
                msg->hdr.m_type, msg->hdr.m_result, msg->hdr.m_cookie, msg->hdr.m_link, msg->hdr.m_len));
        //PRINTF(("process_request : m_result = 0x%x\n", msg->hdr.m_result));
        //PRINTF(("process_request : m_cookie = 0x%x\n", msg->hdr.m_cookie));
        //PRINTF(("process_request : m_link = 0x%x\n", msg->hdr.m_link));
        //PRINTF(("process_request : m_len = 0x%x\n", msg->hdr.m_len));
        if (msg->hdr.m_type == PPP_STATUS) {
            stat = (struct ppp_status *)&msg->data[0];
            PRINTF(("     ----- status = 0x%x", stat->status));
            if (stat->status != PPP_RUNNING) {
                PRINTF((", cause = 0x%x", stat->s.disc.lastDiscCause));
            }
            PRINTF(("\n"));
        }
    }
    else {
        //      PRINTF(("process_request : write nothing on output\n"));
    }

    return 0;
}

/* -----------------------------------------------------------------------------
// msg probably invalid...
----------------------------------------------------------------------------- */
u_long close_cleanup (u_short id, struct msg *msg)
{

    if (!msg)
        msg = &gMsg;
    
    // close fd, in case it was left behind...
    ppp_closefd(id, msg);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFSocketRef AddSocketNativeToRunLoop(int fd)
{
    CFRunLoopSourceRef	rls;
    CFSocketContext	context = { 0, NULL, NULL, NULL, NULL };
    CFSocketRef 	ref;

    PRINTF(("AddSocketNativeToRunLoop...\n"));
    ref = CFSocketCreateWithNative(NULL, fd, kCFSocketReadCallBack,
                                   socketCallback, &context);
    rls = CFSocketCreateRunLoopSource(NULL, ref, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);

    return ref;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int DelSocketRef(CFSocketRef ref)
{
    int 	fd = CFSocketGetNative(ref);

    PRINTF(("DelSocketRef...\n"));
    CFSocketInvalidate(ref);
    CFRelease(ref);
    return fd;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFSocketRef CreateSocketRefWithNative(int fd)
{
    CFSocketContext	context = { 0, NULL, NULL, NULL, NULL };

    return CFSocketCreateWithNative(NULL, fd, kCFSocketReadCallBack,
                                   socketCallback, &context);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFRunLoopSourceRef AddSocketRefToRunLoop(CFSocketRef ref)
{
    CFRunLoopSourceRef	rls;

    rls = CFSocketCreateRunLoopSource(NULL, ref, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);

    return rls;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void DelRunLoopSource(CFRunLoopSourceRef rls)
{
    CFRunLoopSourceInvalidate(rls);
    //CFRunLoopRemoveSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);
}

/* -----------------------------------------------------------------------------
* schedule a timeout to run (once) in 'time' seconds
return the ref to the newly created timer
----------------------------------------------------------------------------- */
CFRunLoopTimerRef AddTimerToRunLoop(void (*func) __P((CFRunLoopTimerRef, void *)), void *arg, u_short time)
{
    CFRunLoopTimerRef		timer;
    CFRunLoopTimerContext	context = { 0, 0, 0, 0, 0 };

    context.info = arg;
    timer = CFRunLoopTimerCreate(0, CFAbsoluteTimeGetCurrent() + time, 0, 0, 0, func, &context);

    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);

    return timer;
}

/* -----------------------------------------------------------------------------
* cancel a scheduled timeout
----------------------------------------------------------------------------- */
void DelTimerFromRunLoop(CFRunLoopTimerRef *timer)
{

    if (*timer) {
        CFRunLoopTimerInvalidate(*timer);
        CFRelease(*timer);
        *timer = 0;
    }
}
