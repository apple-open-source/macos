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
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/dlil.h>
#include <CoreFoundation/CoreFoundation.h>
#define	SYSTEMCONFIGURATION_NEW_API
#include <SystemConfiguration/SystemConfiguration.h>

#include "ppp_msg.h"
#include "../Family/PPP.kmodproj/ppp.h"
#include "../Family/PPP.kmodproj/ppp_domain.h"

#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "chap.h"
#include "upap.h"
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

void link_up_evt(struct ppp *ppp);
void link_down_evt(struct ppp *ppp);
void link_appear_evt(u_char *name, u_short ifunit, u_short subfamily);
void link_disappear_evt(u_char *name, u_short ifunit, u_short subfamily);
void link_willdisappear_evt(u_char *name, u_short ifunit, u_short subfamily);
void link_ringing_evt(struct ppp *ppp);
void link_connecting_evt(struct ppp *ppp);
void link_listening_evt(struct ppp *ppp);
void link_disconnecting_evt(struct ppp *ppp);

int send_ioctl(struct ppp *ppp, u_int32_t io, u_int32_t code, u_int32_t inval, u_int32_t *outval);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

CFSocketRef 			gEvtListenRef;
extern CFSocketRef		gPPPProtoRef;
extern struct msg 		gMsg;

/* -----------------------------------------------------------------------------
initialize the necessary structure for the links
----------------------------------------------------------------------------- */
int link_init()
{
    int 		stat, s;
    struct kev_request  kev_req;

    // we need to initialize our event listener
    // need to listen to PPP events
    // and to if events to detect when new ppp appears

    s = socket(PF_SYSTEM, SOCK_RAW, SYSPROTO_EVENT);
    if (s == -1) {
        // bad...;
        return 1;
    }

    kev_req.vendor_code = KEV_VENDOR_APPLE;
    kev_req.kev_class = KEV_NETWORK_CLASS;
    kev_req.kev_subclass = 0;

    stat = ioctl(s, SIOCSKEVFILT, &kev_req);
    if (stat) {
        // bad...
        return 1;
    }

    printf("link_init, add socket event to loop, %d\n", s);

    // now it's time to add it to the run loop
    gEvtListenRef = AddSocketNativeToRunLoop(s);

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int link_event()
{
    int 			stat, error;
    char	    		buf[1000];
    struct kern_event_msg 	*ev_msg;
    struct ppp 			*ppp;
    struct net_event_data 	*netdata;
    struct kev_ppp_data 	*pppdata;

    stat = recv(CFSocketGetNative(gEvtListenRef), &buf, sizeof(buf), 0);
    if (stat == -1)
        return 1;
    else {

        ev_msg = (struct kern_event_msg *) &buf;
        //printf("event %d\n", ev_msg->event_code);
        //need to check it is coming from the right interface

        switch (ev_msg->kev_subclass) {
            case KEV_DL_SUBCLASS:
                netdata = (struct net_event_data *) &ev_msg->event_data[0];

                if ((netdata->if_family & 0xFFFF) == APPLE_IF_FAM_PPP) {

                    switch (ev_msg->event_code) {
                        case KEV_DL_IF_ATTACHED:
                            printf("link_event, KEV_DL_IF_ATTACHED, %s%ld\n", netdata->if_name, netdata->if_unit);
                            link_appear_evt(netdata->if_name, netdata->if_unit, netdata->if_family >> 16);
                            break;
                        case KEV_DL_IF_DETACHED:
                            printf("link_event, KEV_DL_IF_DETACHED, %s%ld\n", netdata->if_name, netdata->if_unit);
                            link_disappear_evt(netdata->if_name, netdata->if_unit, netdata->if_family >> 16);
                            break;
                        case KEV_DL_IF_DETACHING:
                            printf("link_event, KEV_DL_IF_DETACHING, %s%ld\n", netdata->if_name, netdata->if_unit);
                            link_willdisappear_evt(netdata->if_name, netdata->if_unit, netdata->if_family >> 16);
                            break;
                    }
                }
                    break;

            case KEV_PPP_SUBCLASS:
                pppdata = (struct kev_ppp_data *) &ev_msg->event_data[0];
                ppp = ppp_findbyname(pppdata->link_data.if_name, pppdata->link_data.if_unit);
                if (!ppp)
                    return 1;

                    switch (ev_msg->event_code) {
                        case KEV_PPP_CONNECTED:
                            printf("link_event, KEV_PPP_CONNECTED, %s%d\n", ppp->name, ppp->ifunit);
                            link_up_evt(ppp);
                            break;
                        case KEV_PPP_DISCONNECTED:
                            printf("link_event, KEV_PPP_DISCONNECTED, %s%d\n", ppp->name, ppp->ifunit);
                            link_down_evt(ppp);
                            break;
                        case KEV_PPP_CONNECTING:
                            printf("link_event, KEV_PPP_CONNECTING, %s%d\n", ppp->name, ppp->ifunit);
                            link_connecting_evt(ppp);
                            break;
                        case KEV_PPP_DISCONNECTING:
                            printf("link_event, KEV_PPP_DISCONNECTING, %s%d\n", ppp->name, ppp->ifunit);
                            link_disconnecting_evt(ppp);
                            break;
                        case KEV_PPP_LISTENING:
                            printf("link_event, KEV_PPP_LISTENING, %s%d\n", ppp->name, ppp->ifunit);
                            link_listening_evt(ppp);
                            break;
                        case KEV_PPP_RINGING:
                            printf("link_event, KEV_PPP_RINGING, %s%d\n", ppp->name, ppp->ifunit);
                            link_ringing_evt(ppp);
                            break;
                        case KEV_PPP_NEEDCONNECT:
                            printf("link_event, KEV_PPP_NEEDCONNECT, %s%d\n", ppp->name, ppp->ifunit);

                            if ((ppp->phase == PPP_IDLE) && (isUserLoggedIn() || ppp->connlogout)) {
                                ppp_new_phase(ppp, PPP_INITIALIZE);
                                ppp->status = 0;
                                ppp->lastmsg[0] = 0;
                                ppp->connect_speed = 0;
                                error = link_connect(ppp, 0);
                                if (error) {
                                    ppp_new_phase(ppp, PPP_IDLE);
                                }
                            }
                    }
            }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
this function is currently designed onlyto work with tty
Fix it when connectors are in place.
Set up the serial port with the speed requested by the API.
Speed and other hardware setting will be adjusted by the CCL.
in fact, the speed should be set after the CCL has been played.
----------------------------------------------------------------------------- */
int link_setconfig(struct ppp *ppp)
{
    struct termios tios;

    if (tcgetattr(CFSocketGetNative(ppp->ttyref), &tios) < 0)
        return 1;

    printf("link_setconfig = %d\n", ppp->devspeed);
    if (ppp->devspeed) {
        cfsetospeed(&tios, ppp->devspeed);
        cfsetispeed(&tios, ppp->devspeed);
    }

    if (tcsetattr(CFSocketGetNative(ppp->ttyref), TCSAFLUSH, &tios) < 0)
        return 1;

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int link_getcaps(struct ppp *ppp)
{
    struct ifpppreq    	req;
    int 		err;

    bzero(&req, sizeof(struct ifpppreq));
    sprintf(&req.ifr_name[0], "%s%d", ppp->name, ppp->ifunit);
    req.ifr_code = IFPPP_CAPS;

    if (err = ioctl(CFSocketGetNative(gEvtListenRef), SIOCGIFPPP, (caddr_t) &req))
        return err;

    bcopy(&req.ifr_caps, &ppp->link_caps, sizeof(struct ppp_caps));
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int link_getnblinks(struct ppp *ppp, u_int32_t *nb)
{

    return send_ioctl(ppp, SIOCGIFPPP, IFPPP_NBLINKS, 0, nb);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int link_setnblinks(struct ppp *ppp, u_int32_t nb)
{

    return send_ioctl(ppp, SIOCSIFPPP, IFPPP_NBLINKS, nb, 0);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int link_setloopback(struct ppp *ppp, u_int32_t mode)
{

    if (!(ppp->link_caps.flags & PPP_CAPS_LOOPBACK))
        return EOPNOTSUPP;

    return send_ioctl(ppp, SIOCSIFPPP, IFPPP_LOOPBACK, mode, 0);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int link_getloopback(struct ppp *ppp, u_int32_t *mode)
{

    *mode = 0;

    if (!(ppp->link_caps.flags & PPP_CAPS_LOOPBACK))
        return EOPNOTSUPP;

    return send_ioctl(ppp, SIOCGIFPPP, IFPPP_LOOPBACK, 0, mode);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int link_connect(struct ppp *ppp, u_char afterbusy)
{
    int 		fd;
    char 		prgm[MAXPATHLEN];
    struct ifpppreq    	req;
    struct ifreq 	ifr;

    printf("link_connect, interface = %s%d\n", ppp->name, ppp->ifunit);

    ppp_new_phase(ppp, PPP_CONNECTLINK);

    if (ppp->link_caps.flags & PPP_CAPS_DIAL) {

        if (!link_abort(ppp)) {
            ppp->link_ignore_disc = 1;
        }

        // assure that ethernet interface is up
        bzero(&ifr, sizeof(ifr));
        sprintf(&ifr.ifr_name[0], "%s", ppp->devnam);
        if (ioctl(CFSocketGetNative(gEvtListenRef), SIOCGIFFLAGS, (caddr_t) &ifr) >= 0) {
            ifr.ifr_flags |= IFF_UP;
            ioctl(CFSocketGetNative(gEvtListenRef), SIOCSIFFLAGS, (caddr_t) &ifr);
        }

        // set the configuration, need to handle the full dictionnary
        bzero(&req, sizeof(req));
        sprintf(&req.ifr_name[0], "%s%d", ppp->name, ppp->ifunit);
        req.ifr_code = IFPPP_DEVICE;
        strcpy(req.ifr_device, ppp->devnam);
	ioctl(CFSocketGetNative(gEvtListenRef), SIOCSIFPPP, (caddr_t) &req);

        // build the connection request
        bzero(&req, sizeof(req));
        sprintf(&req.ifr_name[0], "%s%d", ppp->name, ppp->ifunit);
        req.ifr_code = IFPPP_CONNECT;
        // use bcopy because the address could contain zeros
        // bad... change it
        bcopy((ppp->redialstate == redial_alternate) ? ppp->altremoteaddr : ppp->remoteaddr, req.ifr_connect, sizeof(req.ifr_connect));

        return ioctl(CFSocketGetNative(gEvtListenRef), SIOCSIFPPP, (caddr_t) &req);
    }
    else {

        if (!afterbusy) {
            printf("link_connect, not a dial up link, dial device '%s', with ccl '%s'\n", ppp->devnam, ppp->cclname);
    
            fd = open(ppp->devnam, O_NONBLOCK | O_RDWR, 0);
            if (fd < 0)
                return errno;
    
            ppp->ttyref = CreateSocketRefWithNative(fd);
    
            printf("link_connect, ttyfd = %d\n", fd);
    
    #if 0
            if ((fdflags = fcntl(fd, F_GETFL)) == -1
                || fcntl(fd, F_SETFL, fdflags & ~O_NONBLOCK) < 0) {
                //warn(ppp, "Couldn't reset non-blocking mode on device: %m");
                printf("link_connect, Couldn't reset non-blocking mode on device, errno = %d\n", errno);
            }
    #endif
            
            link_setconfig(ppp);
        }
        // now it's time to add it to the run loop
        //ppp->ttyref = AddSocketNativeToRunLoop(fd);
        //ppp->ttyfd = fd;
        
        if (ppp->cclname[0] && ppp->cclprgm[0]) {
            // give additionnal parameters to helpers
            // -v = verbose
            // -c = connect
            // -l %d = link ref currently to use
            // -f %s = file to play
            // -T %s = telephone number substitution
            sprintf(prgm, "%s -m 0 -l %d -f '%s' %s '%s' -s %d -p %d -d %d",
                    ppp->cclprgm,
                    ppp_makeref(ppp),
                    ppp->cclname,
                    ((ppp->redialstate == redial_alternate) ? ppp->altremoteaddr[0] : ppp->remoteaddr[0]) ? "-T" : "",
                    (ppp->redialstate == redial_alternate) ? ppp->altremoteaddr : ppp->remoteaddr,
                    ppp->speaker,
                    ppp->pulse,
                    ppp->dialmode);

            printf("link_connect, will run ccl = %s\n",ppp->cclname);
            printf("link_connect, will run prgm = %s\n",prgm);
            ppp_new_event(ppp, PPP_EVT_CONNSCRIPT_STARTED);

            ppp->cclpid = start_program(ppp->cclprgm, prgm, CFSocketGetNative(ppp->ttyref), CFSocketGetNative(ppp->ttyref), -1);
            if (!ppp->cclpid) {
                ppp->status = PPP_ERR_CONNSCRIPTFAILED;
                link_serial_down(ppp);
                return PPP_ERR_CONNSCRIPTFAILED;
            }
            return 0;
        }

        link_serial_up(ppp);

    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int link_listen(struct ppp *ppp)
{
    struct ifpppreq    	req;

    if (ppp->link_caps.flags & PPP_CAPS_DIAL) {

        bzero(&req, sizeof(req));
        sprintf(&req.ifr_name[0], "%s%d", ppp->name, ppp->ifunit);
        req.ifr_code = IFPPP_LISTEN;
        // use bcopy because the address could contain zeros
        // bad... change it
        bcopy(ppp->listenfilter, req.ifr_listen, sizeof(req.ifr_listen));

        return  ioctl(CFSocketGetNative(gEvtListenRef), SIOCSIFPPP, (caddr_t) &req);
    }
    else {
        return EOPNOTSUPP;
    }

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int link_abort(struct ppp *ppp)
{
    char 	prgm[MAXPATHLEN];

    if (ppp->link_caps.flags & PPP_CAPS_DIAL) {

        return send_ioctl(ppp, SIOCSIFPPP, IFPPP_ABORT, 0, 0);
    }
    else {
        if (ppp->phase == PPP_CONNECTLINK) {
            ppp_new_phase(ppp, PPP_DISCONNECTLINK);

            // first kill the current CCL connector
            ppp_closeclientfd(ppp->unit);

            if (ppp->chatpid) {
                ppp->chatpid = 0;
                DelRunLoopSource(ppp->ttyrls);
                ppp->ttyrls = 0;
            }

            if (ppp->cclname[0] && ppp->cclprgm[0]) {
                // give additionnal parameters to helpers
                // -v = verbose
                // -d = disconnect
                // -l %d = link ref currently to use
                // -f %s = file to play
                sprintf(prgm, "%s -m 1 -l %d -f '%s'",
                        ppp->cclprgm,
                        ppp_makeref(ppp),
                        ppp->cclname);

                //ppp_new_event(ppp, PPP_EVT_CCL_STARTED);

                ppp->cclpid = start_program(ppp->cclprgm, prgm, CFSocketGetNative(ppp->ttyref), CFSocketGetNative(ppp->ttyref), -1);
                if (!ppp->cclpid) {
                    ppp->status = PPP_ERR_DISCSCRIPTFAILED;
                    link_serial_down(ppp);
                    return 1;
                }
                return 0;
            }

            //link_serial_down(ppp); // that will automatically kill ccl engine
        }
    }

    return 0;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int link_disconnect(struct ppp *ppp)
{
    int		err ,dis = 10;

    if (ppp->link_caps.flags & PPP_CAPS_DIAL) {

        ppp_new_phase(ppp, PPP_DISCONNECTLINK);
        return send_ioctl(ppp, SIOCSIFPPP, IFPPP_DISCONNECT, 0, 0);
    }
    else {
        ppp_new_phase(ppp, PPP_DISCONNECTLINK);

        /* Restore old line discipline. */
        ioctl(CFSocketGetNative(ppp->ttyref), TIOCGETD, &dis);
        printf("will disestablich, cur = %d, disc = %d on fd = %d\n", dis, ppp->initdisc, CFSocketGetNative(ppp->ttyref));
        if ((ppp->initdisc >= 0) && (err = ioctl(CFSocketGetNative(ppp->ttyref), TIOCSETD, &ppp->initdisc) < 0)) {
            printf("pb restoring discipline errno %d\n", errno );
            return err;
        }

        ppp->initdisc = -1;

    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void link_up_evt_delayed(CFRunLoopTimerRef timer, void *arg)
{
    struct ppp *ppp = (struct ppp *)arg;

    if (ppp->attached) {
        printf("link_up_evt_delayed\n");
        lcp_lowerup(ppp);
        lcp_open(ppp);		/* Start protocol */
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void link_up_evt(struct ppp *ppp)
{
    u_long	lval;

    printf("link_up\n");

    ppp->link_state = link_connected;

    // open the control socket for read/write to ppp family

    lval = ppp_makeifref(ppp);
    printf("link_up 0x%x\n", lval);
    ioctl(CFSocketGetNative(gPPPProtoRef), IOC_PPP_ATTACHMUX, &lval);
    ppp->attached = 1;

    if (ppp->connect_delay) {
        printf("install delay %s%d\n", ppp->name, ppp->ifunit);
        ppp->link_connectTORef = AddTimerToRunLoop(link_up_evt_delayed, ppp, ppp->connect_delay);
    }
    else
        link_up_evt_delayed(0, ppp);


#if 0
    /* Enable debug in the driver if requested. */
    if (ppp->kdebugflag) {
        if (ioctl(ppp->ttyfd, PPPIOCGFLAGS, (caddr_t) &x) < 0) {
            warn(ppp, "ioctl (PPPIOCGFLAGS): %m");
        } else {
            x |= (ppp->kdebugflag & 0xFF) * SC_DEBUG;
            if (ioctl(ppp->ttyfd, PPPIOCSFLAGS, (caddr_t) &x) < 0)
                warn(ppp, "ioctl(PPPIOCSFLAGS): %m");
        }
    }
#endif

}

/* -----------------------------------------------------------------------------
Turn the serial port into a ppp interface
----------------------------------------------------------------------------- */
void link_serial_up(struct ppp *ppp)
{
    char 	prgm[MAXPATHLEN];
    int 	pppdisc = PPPDISC;

    printf("link_serial_up, %s%d\n", ppp->name, ppp->ifunit);

    if (ppp->cclpid) {
        ppp->cclpid = 0;
        ppp_new_event(ppp, PPP_EVT_CONNSCRIPT_FINISHED);

        if ((ppp->chatmode == PPP_COMM_TERM_SCRIPT 
            ||  ppp->chatmode == PPP_COMM_TERM_WINDOW)
            && exist_file(ppp->chatprgm)) {

            // give additionnal parameters to helpers
            // -v = verbose
            // -l %d = link ref currently to use
            // -f %s = file to play
            // -U %s = username substitution
            // -P %s = password substitution
            // -M 1 = terminal mode
            // -m 0 = mode originate

            sprintf(prgm, "%s -m 0 -M 1 -l %d %s '%s' %s '%s' %s '%s'",
                    ppp->chatprgm,
                    ppp_makeref(ppp),
                    ppp->chatname[0] ? "-f" : "",
                    ppp->chatname[0] ? ppp->chatname : "",
                    ppp->user[0] ? "-U" : "",
                    ppp->user,
                    ppp->passwd[0] ? "-P" : "",
                    ppp->passwd);

            printf("link_connect, will run ccl prgm = %s\n",prgm);

            ppp->ttyrls = AddSocketRefToRunLoop(ppp->ttyref);
            ppp_new_event(ppp, PPP_EVT_TERMSCRIPT_STARTED);

            ppp->chatpid = start_program(ppp->chatprgm, prgm, CFSocketGetNative(ppp->ttyref), CFSocketGetNative(ppp->ttyref), -1);
            if (!ppp->chatpid) {
                ppp->status = PPP_ERR_TERMSCRIPTFAILED;
                DelRunLoopSource(ppp->ttyrls);
                ppp->ttyrls = 0;
                link_serial_down(ppp);
                return;
            }
            
            return;
        }

#if 0
        if (ppp->chatmode == PPP_COMM_TERM_WAIT) {
            // means that there is no program,
            // but we want to wait for a program to contact us
            ppp->chatpid = -1;
            ppp->ttyrls = AddSocketRefToRunLoop(ppp->ttyref);
            ppp_new_event(ppp, PPP_EVT_TERMSCRIPT_STARTED);
            return;
        }
#endif 
    }

    if (ppp->chatpid) {
        ppp->chatpid = 0;
        ppp_new_event(ppp, PPP_EVT_TERMSCRIPT_FINISHED);
        DelRunLoopSource(ppp->ttyrls);
        ppp->ttyrls = 0;
    }

    //DelSocketRefFromRunLoop(ppp->ttyref);

    
    /* Save the old line discipline of fd, and set it to PPP. */
    if (ioctl(CFSocketGetNative(ppp->ttyref), TIOCGETD, &ppp->initdisc) < 0) {
        printf("ioctl TIOCGETD errno %d\n", errno);
    }
    if (ioctl(CFSocketGetNative(ppp->ttyref), TIOCSETD, &pppdisc) < 0) {
        printf("ioctl TIOCSETD errno %d\n", errno);
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void link_serial_down(struct ppp *ppp)
{
    int fd;

    printf("link_serial_down tty = %d\n", CFSocketGetNative(ppp->ttyref));
    ppp_closeclientfd(ppp->unit);
    fd = DelSocketRef(ppp->ttyref);
    //fd = ppp->ttyfd;
    
    if (fd) {
        printf("link_serial_down CLOSE tty = %d\n", fd);
        close(fd);
        ppp->ttyref = 0;
    }
    //link_detach(ppp);
    //info(ppp, LOG_SEPARATOR);
    ppp_reinit(ppp, 3);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void link_down_evt(struct ppp *ppp)
{
    u_long	lval;
    char 	prgm[MAXPATHLEN];

    ppp->link_state = link_disconnected;

    // test if the disconnection event is the result from an abort
    if (ppp->link_ignore_disc) {
        ppp->link_ignore_disc = 0;
        return;
    }

   if (ppp->phase != PPP_DISCONNECTLINK) {
        // unrequested disconnect event
        ppp_new_event(ppp, PPP_EVT_DISCONNECTED);
    }

    // stop the ppp protocol
    lcp_lowerdown(ppp);
    lcp_close(ppp, "");

    // then remove the control socket
    if (ppp->attached) {
        lval = ppp_makeifref(ppp);
        ioctl(CFSocketGetNative(gPPPProtoRef), IOC_PPP_DETACHMUX, &lval);
        ppp->attached = 0;
    }

    if (!(ppp->link_caps.flags & PPP_CAPS_DIAL)) {

        if (ppp->cclname[0] && ppp->cclprgm[0]) {
            // give additionnal parameters to helpers
            // -v = verbose
            // -d = disconnect
            // -l %d = link ref currently to use
            // -f %s = file to play
            sprintf(prgm, "%s -m 1 -l %d -f '%s'",
                    ppp->cclprgm,
                    ppp_makeref(ppp),
                    ppp->cclname );

            ppp->cclpid = start_program(ppp->cclprgm, prgm, CFSocketGetNative(ppp->ttyref), CFSocketGetNative(ppp->ttyref), -1);
            if (!ppp->cclpid) {
                ppp->status = PPP_ERR_DISCSCRIPTFAILED;
                link_serial_down(ppp);
                return;
            }
            return;
        }
        
        link_serial_down(ppp);
    }
    else {
        //info(ppp, LOG_SEPARATOR);
            //link_detach(ppp);
            ppp_reinit(ppp, 3);
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void link_ringing_evt(struct ppp *ppp)
{

    ppp->link_state = link_ringing;

    if (ppp->link_caps.flags & PPP_CAPS_DIAL) {

        ppp->link_state = link_accepting;
        ppp_new_phase(ppp, PPP_CONNECTLINK);
        send_ioctl(ppp, SIOCSIFPPP, IFPPP_ACCEPT, 0, 0);
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void link_connecting_evt(struct ppp *ppp)
{

    ppp->link_state = link_connecting;

    // just in case we lost the DISC event
    ppp->link_ignore_disc = 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void link_listening_evt(struct ppp *ppp)
{

    ppp->link_state = link_listening;
    //ppp_new_phase(ppp, PPP_LISTENING);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void link_disconnecting_evt(struct ppp *ppp)
{

    ppp->link_state = link_listening;
    //ppp_new_phase(ppp, PPP_LISTENING);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void link_appear_evt(u_char *name, u_short ifunit, u_short subfamily)
{

    ppp_appears(name, ifunit, subfamily);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void link_disappear_evt(u_char *name, u_short ifunit, u_short subfamily)
{

   //ppp_dispose(name, ifunit, subfamily);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void link_willdisappear_evt(u_char *name, u_short ifunit, u_short subfamily)
{
    struct ppp *ppp = ppp_findbyname(name, ifunit);

    if (!ppp)
        return;

    //if ((ppp->phase != PPP_IDLE) && (ppp->phase != PPP_LISTENING)) {
    if ((ppp->phase != PPP_IDLE) && (ppp->phase != PPP_DISCONNECTLINK)) {
        link_down_evt(ppp);
    }

    ppp_autoconnect_off(ppp);
    ppp->ifunit = 0xFFFF;
    
}


/* -----------------------------------------------------------------------------
this send an ioctl to the interface, ONLY for u_int32_t arguments
----------------------------------------------------------------------------- */
int send_ioctl(struct ppp *ppp, u_int32_t io, u_int32_t code, u_int32_t inval, u_int32_t *outval)
{
    struct ifpppreq    	req;
    int 		err;

    bzero(&req, sizeof(struct ifpppreq));
    req.ifr_code = code;
    sprintf(req.ifr_name, "%s%d", ppp->name, ppp->ifunit);
    req.ifr_mru = inval; 	// use mru field as they almost all are u_int32_t
    err = ioctl(CFSocketGetNative(gEvtListenRef), io, (caddr_t) &req);
    if (err < 0)
        return err;
    if (outval)
        *outval = req.ifr_mru; // use again mru
    return 0;
}

/* -----------------------------------------------------------------------------
the real ifnet need to be created.
this means load the driver and/or add the new interface to the driver
----------------------------------------------------------------------------- */
int link_attach(struct ppp *ppp)
{
    int 		err;
    struct ifpppreq    	req;

    if (ppp->ifunit != 0xFFFF) 
        return 0;

    /* look if the driver is loaded */
    bzero(&req, sizeof(struct ifpppreq));
    sprintf(&req.ifr_name[0], "%s%d", ppp->name, 0/* ppp->unit */);
    req.ifr_code = IFPPP_CAPS;

    err = ioctl(CFSocketGetNative(gEvtListenRef), SIOCGIFPPP, (caddr_t) &req);
    if (err == 0) {
        // It's loaded, so create a new interface
        bzero(&req, sizeof(struct ifpppreq));
        sprintf(&req.ifr_name[0], "%s%d", ppp->name, 0/* ppp->unit */);
        req.ifr_code = IFPPP_NEWIF;

        err = ioctl(CFSocketGetNative(gEvtListenRef), SIOCSIFPPP, (caddr_t) &req);
        if (err == 0) {
            ppp->ifunit = req.ifr_newif & 0xFFFF;
            printf("link_attach, real interface = %x\n", ppp->ifunit);
            // return 1 if need to wait for event
            if (req.ifr_newif >> 16) 
                return 1;
            
            ppp_appears(ppp->name, ppp->ifunit, ppp->subfamily);
            return 0;
        }
        
        // error
        return -1;
    }

   // find a better way to load the driver...
    switch (ppp->subfamily) {
        case APPLE_IF_FAM_PPP_SERIAL:
            loadKext(KEXT_PPPSERIAL);
            break;
        case APPLE_IF_FAM_PPP_PPPoE:
            loadKext(KEXT_PPPoE);
            break;
    }
    printf("link_attach, (just loaded) real interface = %x\n", 0);
    // let's wait for interface 0 to appear, and reattach.
    ppp->need_attach = 1; // will need to attach the first interface
    ppp->ifunit = 0; // by convention, first unit is 0
    // interface is not yet attached, wait for event
    return 1;
}


/* -----------------------------------------------------------------------------
the ifnet need to be deleted.
----------------------------------------------------------------------------- */
int link_detach(struct ppp *ppp)
{
    int 		err;
    struct ifpppreq    	req;

    if (ppp->ifunit == 0xFFFF) 
        return 0;	// already detached

    bzero(&req, sizeof(struct ifpppreq));
    sprintf(&req.ifr_name[0], "%s%d", ppp->name, ppp->ifunit);
    req.ifr_code = IFPPP_DISPOSEIF;
    req.ifr_disposeif = ppp->ifunit;
    
     printf("link_detach : %d \n", ppp->ifunit);
    err = ioctl(CFSocketGetNative(gEvtListenRef), SIOCSIFPPP, (caddr_t) &req);
    ppp->ifunit = 0xFFFF;
    return 0;

}

