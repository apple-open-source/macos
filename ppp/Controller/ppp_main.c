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
#include <sys/fcntl.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
//#include <SystemConfiguration/SCPrivate.h>      // for SCLog()
#define SCLog

#include "ppp_msg.h"
#include "ppp_privmsg.h"
#include "ppp_client.h"
#include "ppp_manager.h"
#include "ppp_option.h"
#include "ppp_command.h"

/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */

enum {
    do_nothing = 0,
    do_process,
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

CFStringRef 		gPluginsDir = 0;
CFBundleRef 		gBundleRef = 0;
CFURLRef 		gBundleURLRef = 0;
CFStringRef 		gCancelRef = 0;
CFStringRef 		gInternetConnectRef = 0;
CFURLRef 		gIconURLRef = 0;

/* -----------------------------------------------------------------------------
plugin entry points, called by configd
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

    CFRetain(bundle);
}

void prime()
{
    ppp_init_all();
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
    int			s, len, flags;
    CFSocketRef		ref;
    CFRunLoopSourceRef	rls;
    CFSocketContext	context = { 0, NULL, NULL, NULL, NULL };

    len = sizeof(addr);
    if ((s = accept(CFSocketGetNative(inref), (struct sockaddr *) &addr, &len)) == -1)
        return;
        
    PRINTF(("Accepted connection...\n"));

    if ((flags = fcntl(s, F_GETFL)) == -1
	|| fcntl(s, F_SETFL, flags | O_NONBLOCK) == -1) {
        SCLog(TRUE, LOG_INFO, CFSTR("Couldn't set accepting socket in non-blocking mode, errno = %d\n"), errno);
    }
        
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
int
readn(int ref, void *data, int len)
{
    int 	n, left = len;
    void 	*p = data;
    
    while (left > 0) {
        if ((n = read(ref, p, left)) < 0) {
            if (errno == EWOULDBLOCK) 
                return (len - left);
            if (errno != EINTR) 
                return -1;
            n = 0;
        }
        else if (n == 0)
            return -1; /* EOF */
            
        left -= n;
        p += n;
    }
    return (len - left);
}        

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int
writen(int ref, void *data, int len)
{	
    int 	n, left = len;
    void 	*p = data;
    
    while (left > 0) {
        if ((n = write(ref, p, left)) <= 0) {
            if (errno != EINTR) 
                return -1;
            n = 0;
        }
        left -= n;
        p += n;
    }
    return len;
}        

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void clientCallBack(CFSocketRef inref, CFSocketCallBackType type,
                     CFDataRef address, const void *data, void *info)
{
    int 		s = CFSocketGetNative(inref);
    int			action = do_nothing;
    ssize_t		n;
    struct client 	*client;

    client = client_findbysocketref(inref);
    if (client == 0)
        return;
            
    /* first read the header part of the message */
    if (client->msglen < sizeof(struct ppp_msg_hdr)) {
        n = readn(s, &((u_int8_t *)&client->msghdr)[client->msglen], sizeof(struct ppp_msg_hdr) - client->msglen);
        switch (n) {
            case -1:
                action = do_close;
                break;
            default:
                client->msglen += n;
                if (client->msglen == sizeof(struct ppp_msg_hdr)) {
                    client->msgtotallen = client->msglen
                        + client->msghdr.m_len
                        + (client->msghdr.m_flags & USE_SERVICEID ? client->msghdr.m_link : 0);
                    client->msg = CFAllocatorAllocate(NULL, client->msgtotallen + 1, 0);
                    if (client->msg == 0)
                        action = do_error;
                    else {
                        bcopy(&client->msghdr, client->msg, sizeof(struct ppp_msg_hdr));
                        // let's end the message with a null byte
                        client->msg[client->msgtotallen] = 0;
                    }
                }
        }
    }
     
    /* first read the data part of the message, including serviceid */
    if (client->msglen >= sizeof(struct ppp_msg_hdr)) {
        n = readn(s, &client->msg[client->msglen], client->msgtotallen - client->msglen);
        switch (n) {
            case -1:
                action = do_close;
                break;
            default:
                client->msglen += n;
                if (client->msglen == client->msgtotallen) {
                    action = do_process;
                }
        }
    }

    /* perform action */
    switch (action) {
        case do_nothing:
            break;
        case do_error:
        case do_close:
            PRINTF(("Connection closed...\n"));
            /* connection closed by client */
            CFSocketInvalidate(inref);
            client_dispose(client);
            break;

        case do_process:
            // process client request
            processRequest(client, (struct msg *)client->msg);
            CFAllocatorDeallocate(NULL, client->msg);
            client->msg = 0;
            client->msglen = 0;
            client->msgtotallen = 0;
            break;
    }
}

typedef u_long (*msg_function)(struct client *client, struct msg *msg, void **reply);

msg_function requests[] = {
    NULL,			/* */
    ppp_version, 		/* PPP_VERSION */
    ppp_status, 		/* PPP_STATUS */
    ppp_connect, 		/* PPP_CONNECT */
    NULL,			/* */
    ppp_disconnect, 		/* PPP_DISCONNECT */
    ppp_getoption, 		/* PPP_GETOPTION */
    ppp_setoption, 		/* PPP_SETOPTION */
    ppp_enable_event, 		/* PPP_ENABLE_EVENT */
    ppp_disable_event,		/* PPP_DISABLE_EVENT */
    NULL,	 		/* PPP_EVENT */
    ppp_getnblinks, 		/* PPP_GETNBLINKS */
    ppp_getlinkbyindex, 	/* PPP_GETLINKBYINDEX */
    ppp_getlinkbyserviceid, 	/* PPP_GETLINKBYSERVICEID */
    ppp_getlinkbyifname, 	/* PPP_GETLINKBYIFNAME */
    ppp_suspend, 		/* PPP_SUSPEND */
    ppp_resume, 		/* PPP_RESUME */
    ppp_extendedstatus, 	/* PPP_EXTENDEDSTATUS */
    ppp_getconnectdata 		/* PPP_GETCONNECTDATA */
};
#define LAST_REQUEST PPP_GETCONNECTDATA

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long processRequest (struct client *client, struct msg *msg)
{
    void		*reply = 0;
    msg_function	func; 
    
    PRINTF(("process_request : type = %x, len = %d\n", msg->hdr.m_type, msg->hdr.m_len));

    if (msg->hdr.m_type <= LAST_REQUEST) {
        
        func = requests[msg->hdr.m_type];
        if (func)
            (*func)(client, msg, &reply);
    }
    else {
        switch (msg->hdr.m_type) { 
            // private pppd event
            case PPPD_EVENT:
                ppp_event(client, msg);
                break;
        }
    }

    if (msg->hdr.m_len != 0xFFFFFFFF) {

        writen(CFSocketGetNative(client->ref), msg, sizeof(struct ppp_msg_hdr) + 
            (msg->hdr.m_flags & USE_SERVICEID ? msg->hdr.m_link : 0));

       if (msg->hdr.m_len) {
            writen(CFSocketGetNative(client->ref), reply, msg->hdr.m_len);
            CFAllocatorDeallocate(NULL, reply);
        }
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
