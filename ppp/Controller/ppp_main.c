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
#include <sys/un.h>
#include <unistd.h>
#include <net/dlil.h>
#include <sys/param.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
//#include <SystemConfiguration/SCPrivate.h>      // for SCLog()
#define SCLog

#include "ppp_msg.h"
#include "ppp_privmsg.h"
#include "ppp_client.h"
#include "ppp_option.h"
#include "ppp_manager.h"
#include "ppp_command.h"

/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */

enum {
    do_process = 0,
    do_close,
    do_error
};

#define ICON 	"NetworkConnect.icns"

/* -----------------------------------------------------------------------------
forward declarations
----------------------------------------------------------------------------- */


int initThings();
int startListen();

u_long processRequest (struct client *client, struct msg *msg);
u_long close_cleanup (struct client *client, struct msg *msg);

void listenCallBack(CFSocketRef s, CFSocketCallBackType type,
                     CFDataRef address, const void *data, void *info);
void clientCallBack(CFSocketRef s, CFSocketCallBackType type,
                     CFDataRef address, const void *data, void *info);

/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

struct msg 		gMsg;
CFStringRef 		gPluginsDir = 0;
CFBundleRef 		gBundleRef = 0;
CFURLRef 		gBundleURLRef = 0;
CFStringRef 		gCancelRef = 0;
CFStringRef 		gInternetConnectRef = 0;
CFURLRef 		gIconURLRef = 0;

/* -----------------------------------------------------------------------------
plugin entry point, called by configd
----------------------------------------------------------------------------- */
void load(CFBundleRef bundle, Boolean debug)
{
    gBundleRef = bundle;

    if (initThings())
        return;
    if (startListen())
        return;
    if (client_init_all())
        return;
    if (ppp_init_all())
        return;

    CFRetain(bundle);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int initThings()
{
    CFURLRef 		urlref, absurlref;
    
    
    gBundleURLRef = CFBundleCopyBundleURL(gBundleRef);
    
    // create plugins dir
    urlref = CFBundleCopyBuiltInPlugInsURL(gBundleRef);
    if (urlref) {
        absurlref = CFURLCopyAbsoluteURL(urlref);
	if (absurlref) {
            gPluginsDir = CFURLCopyPath(absurlref);
            CFRelease(absurlref);
        }
        CFRelease(urlref);
    }
  
    // create misc notification strings
    gCancelRef = CFBundleCopyLocalizedString(gBundleRef, CFSTR("Cancel"), CFSTR("Cancel"), NULL);
    gInternetConnectRef = CFBundleCopyLocalizedString(gBundleRef, CFSTR("Internet Connect"), CFSTR("Internet Connect"), NULL);
    gIconURLRef = CFBundleCopyResourceURL(gBundleRef, CFSTR(ICON), NULL, NULL);
    return 0;
}
 
/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int startListen ()
{
    struct sockaddr_un	addr;
    int			error, s;
    mode_t		mask;
    CFSocketRef		ref = 0;
    CFRunLoopSourceRef	rls;
    CFSocketContext	context = { 0, NULL, NULL, NULL, NULL };

    if ((s = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1)
        goto fail;

    unlink(PPP_PATH);
    bzero(&addr, sizeof(addr));
    addr.sun_family = AF_LOCAL;
    strcpy(addr.sun_path, PPP_PATH);
    mask = umask(0);
    error = bind(s, (struct sockaddr *)&addr, SUN_LEN(&addr));
    umask(mask);
    if (error) 
        goto fail;

    if ((ref = CFSocketCreateWithNative(NULL, s, kCFSocketReadCallBack,
                                   listenCallBack, &context)) == 0)
        goto fail;
    
    if ((rls = CFSocketCreateRunLoopSource(NULL, ref, 0)) == 0)
        goto fail;
           
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);

    listen(s, SOMAXCONN);
    CFRelease(ref);
    return 0;
    
fail:
    SCLog(TRUE, LOG_INFO, CFSTR("PPPController: initialization failed...\n"));
    if (s != -1) 
        close(s);
    if (ref) {
        CFSocketInvalidate(ref);
        CFRelease(ref);
    }
    return 1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void listenCallBack(CFSocketRef inref, CFSocketCallBackType type,
                     CFDataRef address, const void *data, void *info)
{
    struct sockaddr_un	addr;
    int			s, len;//, flags;
    CFSocketRef		ref;
    CFRunLoopSourceRef	rls;
    CFSocketContext	context = { 0, NULL, NULL, NULL, NULL };

    len = sizeof(addr);
    if ((s = accept(CFSocketGetNative(inref), (struct sockaddr *) &addr, &len)) == -1)
        return;
        
    PRINTF(("Accepted connection...\n"));

//       if ((flags = fcntl(connfd, F_GETFL)) == -1
//           || fcntl(connfd, F_SETFL, flags | O_NONBLOCK) == -1) {
//           printf("Couldn't set accepting socket in non-blocking mode, errno = %d\n", errno);
//       }
        
    if ((ref = CFSocketCreateWithNative(NULL, s, 
                    kCFSocketReadCallBack, clientCallBack, &context)) == 0) {
        close(s);
        return;
    }
    if ((rls = CFSocketCreateRunLoopSource(NULL, ref, 0)) == 0)
        goto fail;

    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);

    if (client_new(ref) == 0)
        goto fail;

    CFRelease(ref);
    return;
    
fail:
    CFSocketInvalidate(ref);
    CFRelease(ref);
    return;

}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void clientCallBack(CFSocketRef inref, CFSocketCallBackType type,
                     CFDataRef address, const void *data, void *info)
{
    int 		s = CFSocketGetNative(inref);
    int			action, len;
    ssize_t		n;
    struct client 	*client;

    client = client_findbysocketref(inref);
    if (client) {

        action = do_process;
        if ((n = read(s, &gMsg, sizeof(struct ppp_msg_hdr))) == 0) {
            action = do_close;
        } else {
            PRINTF(("Data to process... len = %d\n", gMsg.hdr.m_len));
            len = 0;
            if ((gMsg.hdr.m_flags & USE_SERVICEID)
                && gMsg.hdr.m_link) {
                len = gMsg.hdr.m_link;
                if (gMsg.hdr.m_link > MAXDATASIZE) {
                    action = do_error;
                }
                else if (read(s, &gMsg.data[0], gMsg.hdr.m_link) != gMsg.hdr.m_link) {
                    action = do_close;
                }
            }
           if ((action == do_process) && gMsg.hdr.m_len) {
                if ((len + gMsg.hdr.m_len) > MAXDATASIZE) {
                    action = do_error;
                }
                else if (read(s, &gMsg.data[len], gMsg.hdr.m_len) != gMsg.hdr.m_len) {
                    action = do_close;
                }
            }
        }

        switch (action) {
            case do_close:
                PRINTF(("Connection closed...\n"));
                /* connection closed by client */
                CFSocketInvalidate(inref);
                client_dispose(client);
                break;
            case do_error:
                PRINTF(("Connection length error...\n"));
                break;
            case do_process:
                // process client request
                processRequest(client, &gMsg);
                break;
        }
   }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long processRequest (struct client *client, struct msg *msg)
{

    PRINTF(("process_request : type = %x, len = %d\n", msg->hdr.m_type, msg->hdr.m_len));

    switch (msg->hdr.m_type) {
        case PPP_VERSION:
            ppp_version(client, msg);
            break;
        case PPP_STATUS:
            ppp_status(client, msg);
            break;
        case PPP_GETOPTION:
            ppp_getoption(client, msg);
            break;
        case PPP_SETOPTION:
            ppp_setoption(client, msg);
            break;
        case PPP_CONNECT:
            ppp_connect(client, msg);
            break;
        case PPP_DISCONNECT:
            ppp_disconnect(client, msg);
            break;
        case PPP_ENABLE_EVENT:
            ppp_enable_event(client, msg);
            break;
        case PPP_DISABLE_EVENT:
            ppp_disable_event(client, msg);
            break;
        case PPP_GETNBLINKS:
            ppp_getnblinks(client, msg);
            break;
        case PPP_GETLINKBYINDEX:
            ppp_getlinkbyindex(client, msg);
            break;
        case PPP_GETLINKBYSERVICEID:
            ppp_getlinkbyserviceid(client, msg);
            break;
        case PPP_GETLINKBYIFNAME:
            ppp_getlinkbyifname(client, msg);
            break;
        case PPP_SUSPEND:
            ppp_suspend(client, msg);
            break;
        case PPP_RESUME:
            ppp_resume(client, msg);
            break;

        // private pppd event
        case PPPD_EVENT:
            ppp_event(client, msg);
            break;

    }

    if (msg->hdr.m_len != 0xFFFFFFFF) {

        write(CFSocketGetNative(client->ref), msg, sizeof(struct ppp_msg_hdr) + msg->hdr.m_len + 
            (msg->hdr.m_flags & USE_SERVICEID ? msg->hdr.m_link : 0));
        
        PRINTF(("process_request : m_type = 0x%x, result = 0x%x, cookie = 0x%x, link = 0x%x, len = 0x%x\n",
                msg->hdr.m_type, msg->hdr.m_result, msg->hdr.m_cookie, msg->hdr.m_link, msg->hdr.m_len));
#if 0
        if (msg->hdr.m_type == PPP_STATUS) {
            struct ppp_status *stat = (struct ppp_status *)&msg->data[0];
            PRINTF(("     ----- status = 0x%x", stat->status));
            if (stat->status != PPP_RUNNING) {
                PRINTF((", cause = 0x%x", stat->s.disc.lastDiscCause));
            }
            PRINTF(("\n"));
        }
#endif
    }

    return 0;
}
