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
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/route.h>

#define	SYSTEMCONFIGURATION_NEW_API
#include <SystemConfiguration/SystemConfiguration.h>

#include "ppp_msg.h"
#include "ppp_privmsg.h"
#include "../Family/PPP.kmodproj/ppp.h"
#include "../Family/PPP.kmodproj/ppp_domain.h"

#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "chap.h"
#include "upap.h"
#include "auth.h"
#include "ppp_client.h"
#include "ppp_option.h"
#include "ppp_utils.h"
#include "ppp_command.h"
#include "ppp_manager.h"
#include "ppp_utils.h"
#include "link.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

#define IP_FORMAT	"%d.%d.%d.%d"
#define IP_CH(ip)	((u_char *)(ip))
#define IP_LIST(ip)	IP_CH(ip)[0],IP_CH(ip)[1],IP_CH(ip)[2],IP_CH(ip)[3]

/*
 * SET_SA_FAMILY - set the sa_family field of a struct sockaddr,
 * if it exists.
 */
#define SET_SA_FAMILY(addr, family)		\
bzero((char *) &(addr), sizeof(addr));	\
addr.sa_family = (family); 			\
addr.sa_len = sizeof(addr);

#define MAX_IFS		32

/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

TAILQ_HEAD(, ppp) 	ppp_head;
TAILQ_HEAD(, driver) 	driver_head;
CFSocketRef		gPPPProtoRef;
SCDSessionRef		gCfgCache;
extern CFSocketRef 	gEvtListenRef;

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */
int get_ether_addr(struct ppp * ppp, u_int32_t ipaddr, struct sockaddr_dl *hwaddr);
u_int32_t getmask(u_int32_t addr, u_int32_t usermask);
int setipv4address(struct ppp * ppp, u_int32_t o, u_int32_t h, u_int32_t m);
int clearipv4address(struct ppp *ppp, u_int32_t o, u_int32_t h);
int ppp_init_drivers();

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_init_all() {

    int 			flags, s;
    struct sockaddr_pppctl 	pppaddr;
    SCDStatus			status;

    TAILQ_INIT(&ppp_head);
    TAILQ_INIT(&driver_head);

    printf("size of ppp = %ld\n", sizeof(struct ppp));
    printf("MAXPATHLEN = %d\n", MAXPATHLEN);
    printf("sizeof fsm = %ld\n", sizeof(struct fsm));
    printf("sizeof ifnet = %ld\n", sizeof(struct ifnet));
    printf("sizeof client = %ld\n", sizeof(struct client));
    printf("sizeof options = %ld\n", sizeof(struct options));

    /* create a control socket to ppp protocol. */
    s = socket(PF_PPP, SOCK_DGRAM, 0);
    if (s < 0) {
        printf("can't connect to ppp socket, errno = %d\n", errno);
        return 1;
    }

    /* need to connect to it */
    pppaddr.ppp_len = sizeof(struct sockaddr_pppctl);
    pppaddr.ppp_family = AF_PPPCTL;
    pppaddr.ppp_reserved = 0;
    if (connect(s, (struct sockaddr *)&pppaddr, sizeof(struct sockaddr_pppctl)) < 0) {
        printf("can't connect to ppp socket, errno = %d\n", errno);
        close(s);
        return 1;
    }

    /* set socket for non-blocking reads. */
    if ((flags = fcntl(s, F_GETFL)) == -1
        || fcntl(s, F_SETFL, flags | O_NONBLOCK) == -1) {
        printf("Couldn't set ppp_sockfd to non-blocking mode, errno = %d\n", errno);
        close(s);
        return 1;
    }

    /* opens now our session to the cache */
    status = SCDOpen(&gCfgCache, CFSTR("PPPController"));
    if (status != SCD_OK) {
        printf("SCDOpen failed,  %s\n", SCDError(status));
        close(s);
        return 1;
    }

    /* the socket belongs to the run loop */
    gPPPProtoRef = AddSocketNativeToRunLoop(s);

    /* find the PPP driver in installed on the machine */
    ppp_init_drivers();
    
    /* ppp is ready, init the link layer */
    link_init();

    /* read configuration from database */
    options_init_all(gCfgCache);

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_dispose_all()
{
    int 	s;

    options_dispose_all(gCfgCache);

    SCDClose(&gCfgCache);

#if 0 // do it better
    if (ppp->ifaddrs[0] != 0)
        cifaddr(ppp, ppp->ifaddrs[0], ppp->ifaddrs[1]);
    if (ppp->proxy_arp_addr)
        cifproxyarp(ppp, ppp->proxy_arp_addr);
#endif

    s = DelSocketRef(gPPPProtoRef);
    close(s);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int32_t ppp_makeref(struct ppp *ppp)
{
    return (((u_long)ppp->subfamily) << 16) + ppp->unit;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int32_t ppp_makeifref(struct ppp *ppp)
{
    return (((u_long)ppp->subfamily) << 16) + ppp->ifunit;
}

/* -----------------------------------------------------------------------------
PPPSerial and PPPoE are hardcoded until NKE configurator are in place
----------------------------------------------------------------------------- */
int ppp_init_drivers()
{
    struct driver 	*driver;

    /* PPP Serial */
    driver = malloc(sizeof(struct driver));
    if (!driver)
        return 1;	// very bad...

    bzero(driver, sizeof(struct driver));
    TAILQ_INSERT_TAIL(&driver_head, driver, next);

    strncpy(driver->name, APPLE_PPP_NAME_SERIAL, IFNAMSIZ);
    driver->subfamily = APPLE_IF_FAM_PPP_SERIAL;

    strncpy(driver->caps.link_name, "PPP Asynchronous Line Discipline", PPP_NAME_SIZE);
    driver->caps.physical = PPP_PHYS_SERIAL;
    driver->caps.max_mtu = 2048;
    driver->caps.max_mru = 2048;
    driver->caps.flags = PPP_CAPS_ERRORDETECT + PPP_CAPS_ASYNCFRAME + PPP_CAPS_PCOMP + PPP_CAPS_ACCOMP;
    //driver->caps.max_links = 1;

    /* PPPoE */
    driver = malloc(sizeof(struct driver));
    if (!driver)
        return 1;	// very bad...

    bzero(driver, sizeof(struct driver));
    TAILQ_INSERT_TAIL(&driver_head, driver, next);

    strncpy(driver->name, APPLE_PPP_NAME_PPPoE, IFNAMSIZ);
    driver->subfamily = APPLE_IF_FAM_PPP_PPPoE;

    strncpy(driver->caps.link_name, "PPPoE", PPP_NAME_SIZE);
    driver->caps.physical = PPP_PHYS_PPPoE;
    driver->caps.max_mtu = 1492;
    driver->caps.max_mru = 1492;
    driver->caps.flags = PPP_CAPS_DIAL + /*PPP_CAPS_DYNLINK + */PPP_CAPS_LOOPBACK;
    //driver->caps.max_links = 32;
    
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct ppp *ppp_findbyname(u_char *name, u_short ifunit)
{
    struct ppp		*ppp;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if ((ppp->ifunit == ifunit)
            && !strncmp(ppp->name, name, IFNAMSIZ)) {
            return ppp;
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct ppp *ppp_findbyserviceID(u_long serviceID)
{
    struct ppp		*ppp;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if (ppp->serviceID == serviceID) {
            return ppp;
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_setorder(struct ppp *ppp, u_int16_t order)
{

    TAILQ_REMOVE(&ppp_head, ppp, next);
    switch (order) {
        case 0:
            TAILQ_INSERT_HEAD(&ppp_head, ppp, next);
            break;
        case 0xFFFF:
            TAILQ_INSERT_TAIL(&ppp_head, ppp, next);
            break;
        }
}

/* -----------------------------------------------------------------------------
find the ppp structure corresponding to the reference
if ref == -1, then return the default structure (first in the list)
if ref == 
----------------------------------------------------------------------------- */
struct ppp *ppp_findbyref(u_long ref)
{
    u_short		subfam = ref >> 16;
    u_short		unit = ref & 0xFFFF;
    struct ppp		*ppp;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if (((ppp->subfamily == subfam) || (subfam == 0xFFFF))
            &&  ((ppp->unit == unit) || (unit == 0xFFFF))) {
            return ppp;
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_printlist()
{
    struct ppp		*ppp;

    SCDLog(LOG_INFO, CFSTR("Printing list of ppp services : \n"));
    TAILQ_FOREACH(ppp, &ppp_head, next) {
        SCDLog(LOG_INFO, CFSTR("Service : %d, sumfam = %d\n"), ppp->serviceID, ppp->subfamily);
    }
}

/* -----------------------------------------------------------------------------
find the ppp structure corresponding to the reference
if ref == -1, then return the default structure (first in the list)
if ref == 
----------------------------------------------------------------------------- */
struct ppp *ppp_findbyifref(u_long ref)
{
    u_short		subfam = ref >> 16;
    u_short		ifunit = ref & 0xFFFF;
    struct ppp		*ppp;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if (((ppp->subfamily == subfam) || (subfam == 0xFFFF))
            &&  ((ppp->ifunit == ifunit) || (ifunit == 0xFFFF))) {
            return ppp;
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
get the first free ref numer within a family
----------------------------------------------------------------------------- */
u_short ppp_findfreeunit(u_short subfam)
{
    struct ppp		*ppp;
    u_short		unit = 0;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if ((subfam == ppp->subfamily)
            && (ppp->unit == unit)) {
            unit++;
            ppp = TAILQ_FIRST(&ppp_head); // reloop
        }
    }
    return unit;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct driver *ppp_finddriver(u_short subfamily)
{
    struct driver		*driver;

    TAILQ_FOREACH(driver, &driver_head, next) {
        if (driver->subfamily == subfamily) {
            return driver;
        }
    }

    return 0;
}

/* -----------------------------------------------------------------------------
an interface structure needs to be created
unit is the ppp managed unit, not the ifunit 
----------------------------------------------------------------------------- */
struct ppp *ppp_new(u_char *name, u_short unit, u_short subfamily, u_long serviceID)
{
    struct ppp 		*ppp;
    struct driver 	*driver;

    if (ppp = ppp_findbyref((((u_long)subfamily) << 16) + unit))
        return ppp;	// already exist

   ppp = malloc(sizeof(struct ppp));
    if (!ppp)
        return 0;	// very bad...

    bzero(ppp, sizeof(struct ppp));

    TAILQ_INSERT_TAIL(&ppp_head, ppp, next);

    ppp->unit = unit;
    ppp->ifunit = 0xFFFF;		// no real unit yet
    strncpy(ppp->name, name, IFNAMSIZ);
    ppp->subfamily = subfamily;
    ppp->serviceID = serviceID;

    ppp->protocols[0] = &lcp_protent;
    ppp->protocols[1] = &pap_protent;
    ppp->protocols[2] = &chap_protent;
    ppp->protocols[3] = &ipcp_protent;

    driver = ppp_finddriver(subfamily);
    if (!driver) {
        // bad
    }
    else 
        bcopy(&driver->caps, &ppp->link_caps, sizeof(struct ppp_caps));

    ppp_reinit(ppp, 0);

    return ppp;
}

/* -----------------------------------------------------------------------------
user has looged out
need to check the disconnect on logout flag for the ppp interfaces
----------------------------------------------------------------------------- */
int ppp_logout()
{
    struct ppp		*ppp;

    TAILQ_FOREACH(ppp, &ppp_head, next) {
        if (ppp->disclogout) {
            switch (ppp->phase) {
                case PPP_IDLE:
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
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
an interface is come up, need to create the ppp structure
----------------------------------------------------------------------------- */
int ppp_appears(u_char *name, u_short ifunit, u_short subfamily)
{
    struct ppp 		*ppp = ppp_findbyname(name, ifunit);
    int 		error;

printf("ppp_appears, real interface = %s%d\n", name, ifunit);
    // ppp controller only knows about interface present in the database.
    if (!ppp) 
        return 1; 
    if (ppp->need_attach) {
        ppp->need_attach = 0;
        ppp->ifunit = 0xFFFF;	// reset ifnumber
        error = link_attach(ppp);
        if (error) {
            ppp_new_phase(ppp, PPP_IDLE);
        }
    }
    if (ppp->need_connect) {
        ppp->need_connect = 0;
        error = link_connect(ppp, 0);
        if (error) {
            ppp_new_phase(ppp, PPP_IDLE);
        }
    }
    else if (ppp->need_autoconnect) {
        ppp->need_autoconnect = 0;
        ppp_autoconnect_on(ppp);
    }
    else if (ppp->need_autolisten) {
        ppp->need_autolisten = 0;
        error = link_listen(ppp);
        if (error) {
            printf("err listen \n");
            // Fix Me : handle error
            ppp_new_phase(ppp, PPP_IDLE);
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
an interface is come down, dispose the ppp structure
unit is the ppp managed unit, not the ifunit 
----------------------------------------------------------------------------- */
int ppp_dispose(struct ppp *ppp)
{
    struct ppp  	*curdesc, *nextdesc;

    printf("ppp_dispose\n");
    switch (ppp->phase) {
        case PPP_IDLE:
        case PPP_INITIALIZE:
            ppp_reinit(ppp, 3);
            break;
        case PPP_CONNECTLINK:
            ppp->need_dispose = 1;
            link_abort(ppp);
            return 0;
        default:
            ppp->need_dispose = 1;
            link_disconnect(ppp);
            return 0;
    }
        
    nextdesc = TAILQ_FIRST(&ppp_head);
    while (nextdesc) {
        curdesc = nextdesc;
        nextdesc = TAILQ_NEXT(nextdesc, next);
        if (curdesc == ppp) {
            //&& !strncmp(curdesc->name, name, IFNAMSIZ)) {
            //TAILQ_REMOVE(&ppp_head, curdesc, next);

            if (((curdesc)->next.tqe_next) != NULL)
                (curdesc)->next.tqe_next->next.tqe_prev =
                    (curdesc)->next.tqe_prev;
            else
                (ppp_head).tqh_last = (curdesc)->next.tqe_prev;
            *(curdesc)->next.tqe_prev = (curdesc)->next.tqe_next;

            // need to close the protocol first

            // then free the structure
            free(curdesc);
            break;
        }
    }

    return 0;
}

/* -----------------------------------------------------------------------------
reinit the structure. we should not just clear evrything...
----------------------------------------------------------------------------- */
void ppp_autoconnect_on(struct ppp *ppp)
{
  //u_long	loc, dst;
    
    printf("ppp_autoconnect_on \n");
    if (!ppp->demand) {
    printf("ppp_autoconnect_on activate now\n");
        ppp->demand = 1;
        //syslog(LOG_INFO, "______ PPP DIAL ON DEMAND : ON -----\n");
        ppp_attachip(ppp); 
        if (ppp->ipcp_wantoptions.ouraddr == 0) {
            /* make up an arbitrary address for the peer */
            ppp->ipcp_wantoptions.ouraddr = AUTOCONNECT_IP_LOCAL_ADDRESS + ppp_makeref(ppp);
            ppp->ipcp_wantoptions.accept_local = 1; // always accept peer's offer
        }
        if (ppp->ipcp_wantoptions.hisaddr == 0) {
            /* make up an arbitrary address for us */
            ppp->ipcp_wantoptions.hisaddr = AUTOCONNECT_IP_DEST_ADDRESS + ppp_makeref(ppp);
            ppp->ipcp_wantoptions.accept_remote = 1; // always accept peer's offer
        }

        ppp->ipcp_ouraddr = ppp->ipcp_wantoptions.ouraddr;
        ppp->ipcp_hisaddr = ppp->ipcp_wantoptions.hisaddr;
        ppp_addroute(ppp, ppp->ipcp_wantoptions.ouraddr, ppp->ipcp_wantoptions.hisaddr, AUTOCONNECT_IP_MASK_ADDRESS, 
		     ppp->ipcp_wantoptions.hisaddr, 0, OPT_HOSTNAME_DEF);
        ppp_ifup(ppp);
        //syslog(LOG_INFO, "______ PPP DIAL ON DEMAND : ON (Done, host name = '%s') -----\n", OPT_HOSTNAME_DEF);
    }
}

/* -----------------------------------------------------------------------------
reinit the structure. we should not just clear evrything...
----------------------------------------------------------------------------- */
void ppp_autoconnect_off(struct ppp *ppp)
{
  //u_long	loc, dst;

    printf("ppp_autoconnect_off \n");
    if (ppp->demand) {
    printf("ppp_autoconnect_off deactivate now\n");
        ppp->demand = 0;
        //syslog(LOG_INFO, "______ PPP DIAL ON DEMAND : OFF -----\n");
            //loc = AUTOCONNECT_IP_LOCAL_ADDRESS + ppp_makeref(ppp);
            //dst = AUTOCONNECT_IP_DEST_ADDRESS + ppp_makeref(ppp);
            ppp_delroute(ppp, ppp->ipcp_ouraddr, ppp->ipcp_hisaddr);
            ppp->ipcp_ouraddr = ppp->ipcp_hisaddr = 0;
           // ppp_delroute(ppp, ppp->ipcp_wantoptions.ouraddr, ppp->ipcp_wantoptions.hisaddr);
            ppp_detachip(ppp);
            ppp_ifdown(ppp);
        //syslog(LOG_INFO, "______ PPP DIAL ON DEMAND : OFF (Done) -----\n");
    }
}

/* -----------------------------------------------------------------------------
reinit the structure. we should not just clear evrything...
----------------------------------------------------------------------------- */
int ppp_reinit(struct ppp *ppp, u_char rearm)
{
    int 		i, err;
    struct protent 	*protp;

    // first, invalidate timers
    if (ppp->redialTORef) {
        DelTimerFromRunLoop (&ppp->redialTORef);  
        ppp->redialTORef = 0;
    }

    ppp_new_phase(ppp, PPP_IDLE);
    if (ppp->need_dispose) {
    printf("ppp_reinit/dispose\n");
        ppp->need_dispose = 0;
        ppp_dispose(ppp);
        return 0;
    }

   // tester SCDStatus	SCDConsoleUserGet(char *user, int userlen, uid_t *uid,  gid_t *gid));

    if (ppp->def_options.misc.autoconnect.set && ppp->def_options.misc.autoconnect.val) {
        // set need_dialondemand before, because ppp_appears at the end because ppp_appears 
        ppp_apply_options(ppp, &ppp->def_options, 0);
        err = link_attach(ppp);
        switch (err) {
            case 1:
                ppp->need_autoconnect = 1;
                break;
                case 0:
                ppp_autoconnect_on(ppp);
                break;
            default :
                ; // should handle error
        }
    }
    else {
        ppp_autoconnect_off(ppp);
        link_detach(ppp);

        ppp->devnam[0] = 0;
        ppp->devspeed = 0;
        ppp->cclname[0] = 0;
        ppp->chatname[0] = 0;
        ppp->remoteaddr[0] = 0;
        ppp->redialstate = 0;
        ppp->altremoteaddr[0] = 0;
        ppp->listenfilter[0] = 0;
        ppp->link_session_timer = 0;
        ppp->link_idle_timer = 0;
        ppp->connect_delay = 0;
    
        ppp->user[0] = 0;
        ppp->passwd[0] = 0;
        ppp->auth_required = 0;
        ppp->allow_any_ip = 1;
        ppp->auth_idleTORef = 0;
        ppp->auth_sessionTORef = 0;
    
    
        for (i = 0; (protp = ppp->protocols[i]) != NULL; ++i)
            (*protp->init)(ppp);
    
        if (ppp->log_to_fd) {
            close(ppp->log_to_fd);
            ppp->log_to_fd = 0;
        }
    }

        
    if (ppp->def_options.misc.autolisten.set && ppp->def_options.misc.autolisten.val) {
        ppp_apply_options(ppp, &ppp->def_options, 1);
        err = link_attach(ppp);
        switch (err) {
            case 1:
               ppp->need_autolisten = 1;
                break;
            case 0:
               err = link_listen(ppp);
                if (err) {
                    printf("err listen \n");
                    // Fix Me : handle error
                    ppp_new_phase(ppp, PPP_IDLE);
                }
                break;
            default :
                ; // should handle error
        }
    }
    
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int32_t ppp_getaddr(struct ppp *ppp, u_int32_t code, u_int32_t *data)
{
    struct ifreq 		ifr;
    int 			s;
    struct	sockaddr_in 	*addr;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return errno;

    memset (&ifr, 0, sizeof (ifr));
    sprintf(ifr.ifr_name, "%s%d", ppp->name, ppp->ifunit);
    if (ioctl(s, code, &ifr) < 0) {
        close(s);
        return errno;
    }

    close(s);

    addr = (struct sockaddr_in *)&ifr.ifr_addr;
    *data = addr->sin_addr.s_addr;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_cclnote(u_short id, struct msg *msg)
{

    struct ppp	*ppp = ppp_findbyref(msg->hdr.m_link);

    if (ppp) {

        strncpy(ppp->lastmsg, &msg->data[0], sizeof(ppp->lastmsg));
    }

    msg->hdr.m_len = 0xFFFFFFFF; // no reply
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_cclspeed(u_short id, struct msg *msg)
{

    struct ppp	*ppp = ppp_findbyref(msg->hdr.m_link);

    if (ppp) {

        ppp->connect_speed = *(u_int32_t *)msg->data;
    }

    msg->hdr.m_len = 0xFFFFFFFF; // no reply
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_cclwritetext(u_short id, struct msg *msg)
{

    struct ppp	*ppp = ppp_findbyref(msg->hdr.m_link);

    if (ppp) {
        msg->data[msg->hdr.m_len] = 0;
        if (ppp->debug)
            info(ppp, "CCL >> %s", &msg->data[0]);
    }

    msg->hdr.m_len = 0xFFFFFFFF; // no reply
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_cclmatchtext(u_short id, struct msg *msg)
{

    struct ppp	*ppp = ppp_findbyref(msg->hdr.m_link);

    if (ppp) {
        msg->data[msg->hdr.m_len] = 0;
        if (ppp->debug)
            info(ppp, "CCL << %s", &msg->data[0]);
    }

    msg->hdr.m_len = 0xFFFFFFFF; // no reply
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_readfd_data(u_long link, struct msg *msg)
{
    //printf("ppp_readfd data, len = %d, data = %c\n", msg->hdr.m_len, msg->data[0]);

    // notify interested clients
    notifyreaders (link, msg);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_readsockfd_data(struct msg *msg)
{
    u_long 		len = msg->hdr.m_len;
    u_char		*p = &msg->data[0];
    u_short 		protocol, i;
    struct ppp 		*ppp = ppp_findbyifref(*(u_long *)p);
    struct protent 	*protp;

    //printf("ppp_readsockfd_data = %x %x %x %x %x %x %x %x %x %x \n", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);
    p += 4;
    len -= 4;

    if (ppp->debug /*&& (debugflags & DBG_INPACKET)*/) {
        // don't log lcp-echo/reply packets
        if ((*(u_short*)(p + 2) != PPP_LCP)
            || ((*(p + 4) != ECHOREQ) && (*(p + 4) != ECHOREP))) {
            dbglog(ppp, "[%s%d] rcvd %P", ppp->name, ppp->ifunit, p, len);
        }
    }

    p += 2;				/* Skip address and control */
    GETSHORT(protocol, p);
    len -= PPP_HDRLEN;

    /*
     * Toss all non-LCP packets unless LCP is OPEN.
     */
    if (protocol != PPP_LCP && ppp->lcp_fsm.state != OPENED) {
        //MAINDEBUG(("get_input: Received non-LCP packet when LCP not open."));
        return 1;
    }

    /*
     * Until we get past the authentication phase, toss all packets
     * except LCP, LQR and authentication packets.
     */
    if (ppp->phase <= PPP_AUTHENTICATE
        && !(protocol == PPP_LCP || protocol == PPP_LQR
             || protocol == PPP_PAP || protocol == PPP_CHAP)) {
        //MAINDEBUG(("get_input: discarding proto 0x%x in phase %d",
        //         protocol, phase));
        return 1;
    }

    /*
     * Upcall the proper protocol input routine.
     */
    for (i = 0; (protp = ppp->protocols[i]) != NULL; ++i) {
        if (protp->protocol == protocol) {
            (*protp->input)(ppp, p, len);
            return 0;
        }
        if (protocol == (protp->protocol & ~0x8000)
            && protp->datainput != NULL) {
            (*protp->datainput)(ppp, p, len);
            return 0;
        }
    }

    /*    if (debug) {
        const char *pname = protocol_name(protocol);
    if (pname != NULL)
        warn(ppp, "Unsupported protocol '%s' (0x%x) received", pname, protocol);
    else
        warn(ppp, "Unsupported protocol 0x%x received", protocol);
    }
*/
    lcp_sprotrej(ppp, p - PPP_HDRLEN, len + PPP_HDRLEN);

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_apply_options (struct ppp *ppp, struct options *opts, u_char server_mode)
{
    char 			str[256];
    u_long			lval;
    int 			fd;


    /* ----------------------------------------------------------------------
        * first COMM options
        ---------------------------------------------------------------------- */
    if (opts->dev.name.set) {
        ppp->devnam[0] = 0;
        if ((opts->dev.name.str[0] != '/') && (ppp->subfamily==APPLE_IF_FAM_PPP_SERIAL)) {
            strcat(ppp->devnam, DIR_TTYS);
            if ((opts->dev.name.str[0] != 't')
                || (opts->dev.name.str[1] != 't')
                || (opts->dev.name.str[2] != 'y')
                || (opts->dev.name.str[3] != 'd'))
                strcat(ppp->devnam, "cu.");
        }
        strcat(ppp->devnam, opts->dev.name.str);
    }

    if (opts->comm.redialcount.set) {
        ppp->redialcount = opts->comm.redialcount.val;
    }

    if (opts->comm.redialinterval.set) {
        ppp->redialinterval = opts->comm.redialinterval.val;
    }

    if (opts->dev.speed.set) {
        ppp->devspeed = opts->dev.speed.val;
    }

    if (opts->dev.dialmode.set) {
        ppp->dialmode = opts->dev.dialmode.val;
    }

    if (opts->dev.pulse.set) {
        ppp->pulse = opts->dev.pulse.val;
    }

    if (opts->dev.speaker.set) {
        ppp->speaker = opts->dev.speaker.val;
    }

    strcpy(ppp->cclprgm, DIR_HELPERS);
    strcat(ppp->cclprgm, CCL_ENGINE);
#if 0
    if (opts->dev.connectprgm.set) {
        // if scriptname start with /, it's a full path
        // otherwise it's relative to the helpers ppp path (convention)
        ppp->cclprgm[0] = 0;
        if (opts->dev.connectprgm.str[0] != '/')
            strcat(ppp->cclprgm, DIR_HELPERS);
        strcat(ppp->cclprgm, opts->dev.connectprgm.str);
    }
#endif

    if (opts->dev.connectscript.set) {
        // if scriptname start with /, it's a full path
        // otherwise it's relative to the modems folder (convention)
        ppp->cclname[0] = 0;
        if (opts->dev.connectscript.str[0] != '/')
            strcat(ppp->cclname, DIR_MODEMS);
        strcat(ppp->cclname, opts->dev.connectscript.str);
    }

    ppp->chatmode = PPP_COMM_TERM_NONE;
    if (opts->comm.terminalmode.set) {
        ppp->chatmode = opts->comm.terminalmode.val;
    }
    
    strcpy(ppp->chatprgm, DIR_HELPERS);
    switch (ppp->chatmode) {
        case PPP_COMM_TERM_WINDOW:
            strcat(ppp->chatprgm, CHAT_WINDOW);
            break;
        case PPP_COMM_TERM_SCRIPT:
            strcat(ppp->chatprgm, CHAT_ENGINE);
            break;
    }
#if 0
    if (opts->comm.terminalprgm.set && opts->comm.terminalprgm.str[0]) {
        // if chatprgm start with /, it's a full path
        // otherwise it's relative to the helpers ppp path (convention)
        ppp->chatprgm[0] = 0;
        if (opts->comm.terminalprgm.str[0] != '/')
            strcat(ppp->chatprgm, DIR_HELPERS);
        strcat(ppp->chatprgm, opts->comm.terminalprgm.str);
    }
#endif
    ppp->chatname[0] = 0;
    if ((ppp->chatmode == PPP_COMM_TERM_SCRIPT)
        && opts->comm.terminalscript.set && opts->comm.terminalscript.str[0]) {
        // if scriptname start with /, it's a full path
        // otherwise it's relative to the modems folder (convention)
        if (opts->comm.terminalscript.str[0] != '/')
            strcat(ppp->chatname, DIR_CHATS);
        strcat(ppp->chatname, opts->comm.terminalscript.str);
    }

    if (opts->comm.remoteaddr.set && opts->comm.remoteaddr.str[0]) {
        strcpy(ppp->remoteaddr, opts->comm.remoteaddr.str);
    }

    ppp->redialstate = 0;
    ppp->altremoteaddr[0] = 0;
    if (opts->comm.altremoteaddr.set && opts->comm.altremoteaddr.str[0]) {         	strcpy(ppp->altremoteaddr, opts->comm.altremoteaddr.str);
    }

    if (opts->comm.listenfilter.set && opts->comm.listenfilter.str[0]) {
        strcpy(ppp->listenfilter, opts->comm.listenfilter.str);
    }

    if (opts->comm.sessiontimer.set && opts->comm.sessiontimer.val) {
        ppp->link_session_timer = opts->comm.sessiontimer.val;
    }

    if (opts->comm.idletimer.set && opts->comm.idletimer.val) {
        ppp->link_idle_timer = opts->comm.idletimer.val;
    }

    if (opts->comm.connectdelay.set && opts->comm.connectdelay.val) {
        ppp->connect_delay = opts->comm.connectdelay.val;
    }

    /* ----------------------------------------------------------------------
        * then LCP options
        ---------------------------------------------------------------------- */
    // set echo option, so ppp hangup if we pull the modem cable
    // echo option is 2 bytes for interval + 2 bytes for failure
    if (opts->lcp.echo.set && (opts->lcp.echo.val & 0xffff0000) && (opts->lcp.echo.val & 0xffff)) {
        ppp->lcp_echo_interval = opts->lcp.echo.val >> 16;
        ppp->lcp_echo_fails = opts->lcp.echo.val & 0xffff;
    }


    ppp->lcp_wantoptions.neg_pcompression = 0;
    ppp->lcp_allowoptions.neg_pcompression = 0;
    if (opts->lcp.pcomp.set && (ppp->link_caps.flags & PPP_CAPS_PCOMP)) {
        ppp->lcp_wantoptions.neg_pcompression = opts->lcp.pcomp.val ? 1 : 0;
        ppp->lcp_allowoptions.neg_pcompression =  opts->lcp.pcomp.val ? 1 : 0;
    }
    ppp->lcp_wantoptions.neg_accompression = 0;
    ppp->lcp_allowoptions.neg_accompression = 0;
    if (opts->lcp.accomp.set && (ppp->link_caps.flags & PPP_CAPS_ACCOMP)) {
        ppp->lcp_wantoptions.neg_accompression = opts->lcp.accomp.val ? 1 : 0;
        ppp->lcp_allowoptions.neg_accompression =  opts->lcp.accomp.val ? 1 : 0;
    }
    if (opts->lcp.mru.set) {
        ppp->lcp_wantoptions.neg_mru = 1;
        ppp->lcp_wantoptions.mru = opts->lcp.mru.val;
        ppp->lcp_allowoptions.neg_mru =  1;
        ppp->lcp_allowoptions.mru =  opts->lcp.mru.val;
    }
    if (opts->lcp.mtu.set) {
        ppp->lcp_allowoptions.neg_mru =  1;
        ppp->lcp_allowoptions.mru =  opts->lcp.mtu.val;
    }

    ppp->lcp_wantoptions.neg_asyncmap = 0;
    ppp->lcp_allowoptions.neg_asyncmap = 0;
    if (opts->lcp.rcaccm.set && (ppp->link_caps.flags & PPP_CAPS_ASYNCFRAME)) {
        ppp->lcp_wantoptions.neg_asyncmap = 1;
	ppp->lcp_allowoptions.neg_asyncmap = 1;
    }

    if (opts->lcp.txaccm.set && (ppp->link_caps.flags & PPP_CAPS_ASYNCFRAME)) {
        if (opts->lcp.txaccm.val) {
            for (lval = 0; lval < 32; lval++) {
                if ((opts->lcp.txaccm.val >> lval) & 1) {
                    ppp->lcp_xmit_accm[lval >> 5] |= 1 << (lval & 0x1F);
                    //printf("ppp->lcp_xmit_accm (%d) = 0x%x\n", lval >> 5, ppp->lcp_xmit_accm[lval >> 5]);
                }
            }
        }
    }

    /* ----------------------------------------------------------------------
        * then AUTH options
        ---------------------------------------------------------------------- */

    ppp->auth_required = 0;
    if (server_mode) {

        if (opts->auth.proto.set) {
            switch (opts->auth.proto.val) {
                case PPP_AUTH_NONE:
                    break;
                case PPP_AUTH_PAPCHAP:
                    ppp->auth_required = 1;
                    ppp->lcp_wantoptions.neg_chap = 1; // will start with chap,
                    ppp->lcp_wantoptions.neg_upap = 1; // then do pap if chap is refused
                    break;
                case PPP_AUTH_CHAP:
                    ppp->auth_required = 1;
                    ppp->lcp_wantoptions.neg_chap = 1;
                    break;
                case PPP_AUTH_PAP:
                    ppp->auth_required = 1;
                    ppp->lcp_wantoptions.neg_upap = 1;
                    break;
            }
        }
    }
    else {
        //if (opts->auth.proto.set && (opts->auth.proto.val != PPP_AUTH_NONE)) {

        if (opts->auth.name.set && opts->auth.name.str[0]) {
            strcpy(ppp->user, opts->auth.name.str);
        }

        if (opts->auth.passwd.set && opts->auth.passwd.str[0]) {
            strcpy(ppp->passwd, opts->auth.passwd.str);
        }
        //}
    }


/* ----------------------------------------------------------------------
* then IPCP options
---------------------------------------------------------------------- */
if (opts->ipcp.hdrcomp.set && (ppp->link_caps.flags & PPP_CAPS_ERRORDETECT)) {
    ppp->ipcp_wantoptions.neg_vj = opts->ipcp.hdrcomp.val ? 1 : 0;
    ppp->ipcp_allowoptions.neg_vj =  opts->ipcp.hdrcomp.val ? 1 : 0;
}
// temporary disable it
//ppp->ipcp_wantoptions.neg_vj = 0;
//ppp->ipcp_allowoptions.neg_vj =  0;
//

    ppp->ipcp_disable_defaultip = !(opts->ipcp.localaddr.set && opts->ipcp.localaddr.val);
    
    if ((opts->ipcp.localaddr.set && opts->ipcp.localaddr.val)  || (opts->ipcp.remoteaddr.set && opts->ipcp.remoteaddr.val)) {
        if (opts->ipcp.localaddr.set && opts->ipcp.localaddr.val) {
            ppp->ipcp_wantoptions.ouraddr = opts->ipcp.localaddr.val;
            ppp->ipcp_allowoptions.ouraddr = opts->ipcp.localaddr.val;
            if (!server_mode)
                ppp->ipcp_wantoptions.accept_local = 1; // always accept peer's offer
        }
        if (opts->ipcp.remoteaddr.set && opts->ipcp.remoteaddr.val) {
            ppp->ipcp_wantoptions.hisaddr = opts->ipcp.remoteaddr.val;
            ppp->ipcp_allowoptions.hisaddr = opts->ipcp.remoteaddr.val;
            if (!server_mode)
                ppp->ipcp_wantoptions.accept_remote = 1; // always accept peer's offer
        }
    }

if (server_mode) {
    if (opts->ipcp.serverdns1.set && opts->ipcp.serverdns1.val) {
        ppp->ipcp_allowoptions.dnsaddr[0] = opts->ipcp.serverdns1.val;
    }

    if (opts->ipcp.serverdns1.set && opts->ipcp.serverdns2.val) {
        ppp->ipcp_allowoptions.dnsaddr[1] = opts->ipcp.serverdns2.val;
    }
}
else {
    ppp->ipcp_usepeerdns = 0;
    if (opts->ipcp.useserverdns.set) {
        ppp->ipcp_usepeerdns = opts->ipcp.useserverdns.val;
    }
}


/* ----------------------------------------------------------------------
* then MISC options
---------------------------------------------------------------------- */

    if (opts->misc.verboselog.set) {
        ppp->debug = *(u_long *)&opts->misc.verboselog.val;
    }

    if (opts->comm.loopback.set) {
        link_setloopback(ppp, *(u_long *)&opts->comm.loopback.val);
    }
    
    if (opts->misc.disclogout.set) {
        ppp->disclogout = opts->misc.disclogout.val;
    }

    if (opts->misc.connlogout.set) {
        ppp->connlogout = opts->misc.connlogout.val;
    }

if (opts->misc.logfile.set && opts->misc.logfile.str[0]) {
    // if logfile start with /, it's a full path
    // otherwise it's relative to the logs folder (convention)
    // we also strongly advise to name the file with the link number
    // for example ppplog0
    // the default path is /var/log
    // it's useful to have the debug option with the logfile option
    // with debug option, pppd will log the negociation
    // debug option is different from kernel debug trace

    str[0] = 0;
    if (opts->misc.logfile.str[0] != '/')
        strcat(str, DIR_LOGS);
    strcat(str, opts->misc.logfile.str);

    fd = open(str, O_WRONLY | O_APPEND | O_CREAT | O_EXCL, 0644);
    if ((fd < 0) && (errno == EEXIST))
        fd = open(str, O_WRONLY | O_APPEND);
    if (fd)
        ppp->log_to_fd = fd;
}

return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void ppp_redialTimeout (CFRunLoopTimerRef timer, void *arg)
{
    struct ppp	*ppp = (struct ppp *)arg;

    //DelTimerFromRunLoop(&ppp->redialTORef);
    ppp->redialTORef = 0;
    if (!ppp->altremoteaddr[0])
    	 ppp->redialcount--;
    link_connect(ppp, 1);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_cclresult(u_short id, struct msg *msg)
{
    u_long result = *(u_long *)&msg->data[0];
    u_long cause = 0, done;
    struct ppp	*ppp = ppp_findbyref(msg->hdr.m_link);

    printf("ppp_cclresult = %ld, link %d, phase = %d\n", result, msg->hdr.m_link, ppp->phase);

    if (!ppp) {
        msg->hdr.m_len = 0xFFFFFFFF; // no reply
        return 0;
    }

    switch (ppp->phase) {
        case PPP_DISCONNECTLINK:
        case PPP_TERMINATE:
            link_serial_down(ppp);
            msg->hdr.m_len = 0xFFFFFFFF; // no reply
            return 0;
        case PPP_CONNECTLINK:
            break;
        default:
            return 0;
    }
    
    switch (result) {
        case 0 :	// ok
        case cclErr_ExitOK:
            result = 0;
            link_serial_up(ppp);
            break;
        case cclErr_ScriptCancelled:
            // is it useful to add a USERCANCEL error ?
            // cause = PPP_ERR_USERCANCEL;
            break;
        case cclErr_ModemErr:
            cause = PPP_ERR_MOD_ERROR;
            break;
        case cclErr_LineBusyErr:
            cause = PPP_ERR_MOD_BUSY;
            if (ppp->redialcount) {
                done = 0;
                if (ppp->altremoteaddr[0] && (ppp->redialstate != redial_alternate)) {
                    if (ppp->redialstate)
                        ppp->redialcount--;
                    ppp->redialstate = redial_alternate;
                    done = (link_connect(ppp, 1) == 0);

                }
                if (!done) {
                    ppp->redialstate = redial_main;
                    ppp->redialTORef = AddTimerToRunLoop(ppp_redialTimeout, ppp, ppp->redialinterval);
                }
                result = 0;
            }
            break;
        case cclErr_NoCarrierErr:
            cause = PPP_ERR_MOD_NOCARRIER;
            break;
        case cclErr_NoDialTone:
            cause = PPP_ERR_MOD_NODIALTONE;
            break;
        default:
            cause = ppp->cclpid ? PPP_ERR_CONNSCRIPTFAILED:PPP_ERR_TERMSCRIPTFAILED;
            break;
    }

    // lastcause always reflect the last disconnexion cause
    // curcause reflect the cause currently associated with the current state
    // ccl and ppp share the same cause
    ppp->status = cause;
    if (result) {
        ppp_disconnect(id, msg);
        //link_serial_down(ppp);
    }

    msg->hdr.m_len = 0xFFFFFFFF; // no reply
    return 0;
}

/* -----------------------------------------------------------------------------
* ppp_new_phase - signal the start of a new phase of pppd's operation.
----------------------------------------------------------------------------- */
void ppp_new_phase(struct ppp *ppp, u_int16_t phase)
{
    ppp->phase = phase;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_new_event(struct ppp *ppp, u_int32_t event)
{
    u_int32_t		cause = 0;

    //printf("ppp_new_event, event = 0x%x\n", event);

    if (event == PPP_EVT_DISCONNECTED) {
        cause = ppp->status;
    }

    client_notify(ppp_makeref(ppp), event, cause);
}


/* -----------------------------------------------------------------------------
Output PPP packet
----------------------------------------------------------------------------- */
void ppp_output(struct ppp *ppp, u_char *p, int len)
{

    if (ppp->debug) {
        // don't log lcp-echo/reply packets
        if ((*(u_short*)(p + 2) != PPP_LCP)
            || (((*(p + 4) != ECHOREQ) && (*(p + 4) != ECHOREP)))) {
            dbglog(ppp, "[%s%d] sent %P", ppp->name, ppp->ifunit, p, len);
        }
    }

    /******* FIX ME *******/
    /* this code assupe that p points to the output_buf field in ppp structure */
    p -= 4;
    *(u_int16_t *)p = ppp->subfamily;
    *(u_int16_t *)(p + 2) = ppp->ifunit;
    len += 4;

    if (write(CFSocketGetNative(gPPPProtoRef), p, len) < 0) {

        if (errno != EIO)
            ;//error(ppp, "write: %m");
    }
}

/* -----------------------------------------------------------------------------
return how long the link has been idle
----------------------------------------------------------------------------- */
int ppp_get_idle_time(struct ppp *ppp, struct ppp_idle *idle)
{
    struct ifpppreq 	ifpppr;

    sprintf(ifpppr.ifr_name, "%s%d", ppp->name, ppp->ifunit);
    ifpppr.ifr_code = IFPPP_IDLE;
    if  (ioctl(CFSocketGetNative(gEvtListenRef), SIOCGIFPPP, (caddr_t) &ifpppr) < 0)
        return 1;

    idle->xmit_idle = ifpppr.ifr_idle.xmit_idle;
    idle->recv_idle = ifpppr.ifr_idle.recv_idle;
    return 0;
}



/*
 * sifproxyarp - Make a proxy ARP entry for the peer.
 */
static struct {
    struct rt_msghdr		hdr;
    struct sockaddr_inarp	dst;
    struct sockaddr_dl		hwa;
    char			extra[128];
} arpmsg;

static int arpmsg_valid;

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_setipv4proxyarp(struct ppp *ppp, u_int32_t hisaddr)
{
    int routes;

    /*
     * Get the hardware address of an interface on the same subnet
     * as our local address.
     */
    memset(&arpmsg, 0, sizeof(arpmsg));
    if (!get_ether_addr(ppp, hisaddr, &arpmsg.hwa)) {
        ;//error(ppp, "Cannot determine ethernet address for proxy ARP");
        return 0;
    }

    if ((routes = socket(PF_ROUTE, SOCK_RAW, AF_INET)) < 0) {
        ;//error(ppp, "Couldn't add proxy arp entry: socket: %m");
        return 0;
    }

    arpmsg.hdr.rtm_type = RTM_ADD;
    arpmsg.hdr.rtm_flags = RTF_ANNOUNCE | RTF_HOST | RTF_STATIC;
    arpmsg.hdr.rtm_version = RTM_VERSION;
    arpmsg.hdr.rtm_seq = ++ppp->rtm_seq;
    arpmsg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY;
    arpmsg.hdr.rtm_inits = RTV_EXPIRE;
    arpmsg.dst.sin_len = sizeof(struct sockaddr_inarp);
    arpmsg.dst.sin_family = AF_INET;
    arpmsg.dst.sin_addr.s_addr = hisaddr;
    arpmsg.dst.sin_other = SIN_PROXY;

    arpmsg.hdr.rtm_msglen = (char *) &arpmsg.hwa - (char *) &arpmsg
        + arpmsg.hwa.sdl_len;
    if (write(routes, &arpmsg, arpmsg.hdr.rtm_msglen) < 0) {
        ;//error(ppp, "Couldn't add proxy arp entry: %m");
        close(routes);
        return 0;
    }

    close(routes);
    arpmsg_valid = 1;
    ppp->proxy_arp_addr = hisaddr;
    return 1;
}

/* -----------------------------------------------------------------------------
Delete the proxy ARP entry for the peer
----------------------------------------------------------------------------- */
int ppp_clearipv4proxyarp(struct ppp *ppp, u_int32_t hisaddr)
{
    int routes;

    if (!arpmsg_valid)
        return 0;
    arpmsg_valid = 0;

    arpmsg.hdr.rtm_type = RTM_DELETE;
    arpmsg.hdr.rtm_seq = ++ppp->rtm_seq;

    if ((routes = socket(PF_ROUTE, SOCK_RAW, AF_INET)) < 0) {
        ;//error(ppp, "Couldn't delete proxy arp entry: socket: %m");
        return 0;
    }

    if (write(routes, &arpmsg, arpmsg.hdr.rtm_msglen) < 0) {
        ;//error(ppp, "Couldn't delete proxy arp entry: %m");
        close(routes);
        return 0;
    }

    close(routes);
    ppp->proxy_arp_addr = 0;
    return 1;
}

/*
 * get_ether_addr - get the hardware address of an interface on the
 * the same subnet as ipaddr.
 */

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int get_ether_addr(struct ppp * ppp, u_int32_t ipaddr, struct sockaddr_dl *hwaddr)
{
    struct ifreq *ifr, *ifend, *ifp;
    u_int32_t ina, mask;
    struct sockaddr_dl *dla;
    struct ifreq ifreq;
    struct ifconf ifc;
    struct ifreq ifs[MAX_IFS];
    int 		s;

    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        ;//fatal("Couldn't create IP socket: %m");

        ifc.ifc_len = sizeof(ifs);
        ifc.ifc_req = ifs;
        if (ioctl(s, SIOCGIFCONF, &ifc) < 0) {
            ;//error(ppp, "ioctl(SIOCGIFCONF): %m");
            close(s);
            return 0;
        }

        /*
         * Scan through looking for an interface with an Internet
         * address on the same subnet as `ipaddr'.
         */
        ifend = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);
        for (ifr = ifc.ifc_req; ifr < ifend; ifr = (struct ifreq *)
             ((char *)&ifr->ifr_addr + ifr->ifr_addr.sa_len)) {
            if (ifr->ifr_addr.sa_family == AF_INET) {
                ina = ((struct sockaddr_in *) &ifr->ifr_addr)->sin_addr.s_addr;
                strlcpy(ifreq.ifr_name, ifr->ifr_name, sizeof(ifreq.ifr_name));
                /*
                 * Check that the interface is up, and not point-to-point
                 * or loopback.
                 */
                if (ioctl(s, SIOCGIFFLAGS, &ifreq) < 0)
                    continue;
                if ((ifreq.ifr_flags &
                     (IFF_UP|IFF_BROADCAST|IFF_POINTOPOINT|IFF_LOOPBACK|IFF_NOARP))
                    != (IFF_UP|IFF_BROADCAST))
                    continue;
                /*
                 * Get its netmask and check that it's on the right subnet.
                 */
                if (ioctl(s, SIOCGIFNETMASK, &ifreq) < 0)
                    continue;
                mask = ((struct sockaddr_in *) &ifreq.ifr_addr)->sin_addr.s_addr;
                if ((ipaddr & mask) != (ina & mask))
                    continue;

                break;
            }
        }

        if (ifr >= ifend) {
            close(s);
            return 0;
        }
        //info("found interface %s for proxy arp", ifr->ifr_name);

        /*
         * Now scan through again looking for a link-level address
         * for this interface.
         */
        ifp = ifr;
        for (ifr = ifc.ifc_req; ifr < ifend; ) {
            if (strcmp(ifp->ifr_name, ifr->ifr_name) == 0
                && ifr->ifr_addr.sa_family == AF_LINK) {
                /*
                 * Found the link-level address - copy it out
                 */
                dla = (struct sockaddr_dl *) &ifr->ifr_addr;
                BCOPY(dla, hwaddr, dla->sdl_len);
                close(s);
                return 1;
            }
            ifr = (struct ifreq *) ((char *)&ifr->ifr_addr + ifr->ifr_addr.sa_len);
        }

        close(s);
        return 0;
}



/* -----------------------------------------------------------------------------
configure the transmit characteristics of the ppp interface
----------------------------------------------------------------------------- */
void ppp_send_config(struct ppp *ppp, int mtu, u_int32_t asyncmap, int pcomp, int accomp)
{
    u_long 		x;
    struct ifpppreq 	ifpppr;
    struct ifreq   	ifr;
    int			err;

    // mtu for ppp0
    sprintf(ifr.ifr_name, "%s%d", ppp->name, ppp->ifunit);
    ifr.ifr_mtu = mtu;
    err = ioctl(CFSocketGetNative(gEvtListenRef), SIOCSIFMTU, (caddr_t) &ifr);

    if (ppp->link_caps.flags & PPP_CAPS_ASYNCFRAME) {
        sprintf(ifpppr.ifr_name, "%s%d", ppp->name, ppp->ifunit);
        ifpppr.ifr_code = IFPPP_ASYNCMAP;
        ifpppr.ifr_map = asyncmap;
        err = ioctl(CFSocketGetNative(gEvtListenRef), SIOCSIFPPP, (caddr_t) &ifpppr);
    }

    ifpppr.ifr_code = IFPPP_EFLAGS;
    err = ioctl(CFSocketGetNative(gEvtListenRef), SIOCGIFPPP, (caddr_t) &ifpppr);
    if (err >= 0) {
        x = ifpppr.ifr_eflags;
        x = accomp ? x | IFF_PPP_DOES_ACCOMP: x & ~IFF_PPP_DOES_ACCOMP;
        x = pcomp ? x | IFF_PPP_DOES_PCOMP: x & ~IFF_PPP_DOES_PCOMP;
        ifpppr.ifr_eflags = x;
        ioctl(CFSocketGetNative(gEvtListenRef), SIOCSIFPPP, (caddr_t) &ifpppr);
    }

}


/* -----------------------------------------------------------------------------
set the extended transmit ACCM for the interface
----------------------------------------------------------------------------- */
void ppp_set_xaccm(struct ppp *ppp, ext_accm accm)
{
    struct ifpppreq ifpppr;

    if (ppp->link_caps.flags & PPP_CAPS_ASYNCFRAME) {

        sprintf(ifpppr.ifr_name, "%s%d", ppp->name, ppp->ifunit);
        ifpppr.ifr_code = IFPPP_XASYNCMAP;
        bcopy(&accm[0], &ifpppr.ifr_xmap[0], sizeof(ext_accm));
        ioctl(CFSocketGetNative(gEvtListenRef), SIOCSIFPPP, (caddr_t) &ifpppr);
    }
}

/* -----------------------------------------------------------------------------
configure the receive-side characteristics of the ppp interface
----------------------------------------------------------------------------- */
void ppp_recv_config(struct ppp *ppp, int mru, u_int32_t asyncmap, int pcomp, int accomp)
{
    u_long 		x;
    struct ifpppreq 	ifpppr;
    int 		err;

    // mru for the link
    sprintf(ifpppr.ifr_name, "%s%d", ppp->name, ppp->ifunit);
    ifpppr.ifr_code = IFPPP_MRU;
    ifpppr.ifr_mru = mru;
    err = ioctl(CFSocketGetNative(gEvtListenRef), SIOCSIFPPP, (caddr_t) &ifpppr);

    if (ppp->link_caps.flags & PPP_CAPS_ASYNCFRAME) {
        ifpppr.ifr_code = IFPPP_RASYNCMAP;
        ifpppr.ifr_map = asyncmap;
        err = ioctl(CFSocketGetNative(gEvtListenRef), SIOCGIFPPP, (caddr_t) &ifpppr);
    }

    ifpppr.ifr_code = IFPPP_EFLAGS;
    err = ioctl(CFSocketGetNative(gEvtListenRef), SIOCGIFPPP, (caddr_t) &ifpppr);
    if (err >= 0) {
        x = ifpppr.ifr_eflags;
        x = accomp ? x | IFF_PPP_ACPT_ACCOMP: x & ~IFF_PPP_ACPT_ACCOMP;
        x = pcomp ? x | IFF_PPP_ACPT_PCOMP: x & ~IFF_PPP_ACPT_PCOMP;
        ifpppr.ifr_eflags = x;
        ioctl(CFSocketGetNative(gEvtListenRef), SIOCSIFPPP, (caddr_t) &ifpppr);
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_attachip(struct ppp * ppp)
{
    struct ifpppreq 	ifpppr;
    int 		err;

    // mru for the link
    sprintf(ifpppr.ifr_name, "%s%d", ppp->name, ppp->ifunit);
    ifpppr.ifr_code = IFPPP_ATTACH_IP;
    err = ioctl(CFSocketGetNative(gEvtListenRef), SIOCSIFPPP, (caddr_t) &ifpppr);

    return (err < 0);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_detachip(struct ppp * ppp)
{
    struct ifpppreq 	ifpppr;
    int 		err;

    // mru for the link
    sprintf(ifpppr.ifr_name, "%s%d", ppp->name, ppp->ifunit);
    ifpppr.ifr_code = IFPPP_DETACH_IP;
    err = ioctl(CFSocketGetNative(gEvtListenRef), SIOCSIFPPP, (caddr_t) &ifpppr);

    return (err < 0);
}



/* -----------------------------------------------------------------------------
config tcp header compression
----------------------------------------------------------------------------- */
int ppp_set_ipvjcomp(struct ppp * ppp,  int vjcomp, int cidcomp, int maxcid)
{
    struct ifpppreq 	ifpppr;
    int err;
    int 		s;

    s = socket(PF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return 0;

    printf("ppp_set_ipvjcomp, %s%d, vjcomp %d, cidcomp %d, macxid %d\n", ppp->name, ppp->unit, vjcomp, cidcomp, maxcid);

    bzero(&ifpppr, sizeof(struct ifpppreq));
    sprintf(ifpppr.ifr_name, "%s%d", ppp->name, ppp->ifunit);
    ifpppr.ifr_code = IFPPP_IP_VJ;
    ifpppr.ifr_ip_vj.vj = vjcomp ? 1 : 0;
    ifpppr.ifr_ip_vj.cid = cidcomp ? 1 : 0;
    ifpppr.ifr_ip_vj.max_cid = maxcid;

    err = ioctl(s, SIOCSIFPPP, (caddr_t) &ifpppr);
    if (err < 0)
        printf("ppp_set_ipvjcomp errno %d\n", errno);

    close(s);
    return 1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_ifup(struct ppp *ppp)
{
    struct ifreq ifr;
    int 	s;
    
    s = socket(PF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return 0;
    
    sprintf(ifr.ifr_name, "%s%d", ppp->name, ppp->ifunit);
    if (ioctl(s, SIOCGIFFLAGS, (caddr_t) &ifr) < 0) {
	//error("ioctl (SIOCGIFFLAGS): %m");
            close(s);
	return 0;
    }
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(s, SIOCSIFFLAGS, (caddr_t) &ifr) < 0) {
	//error("ioctl(SIOCSIFFLAGS): %m");
            close(s);
	return 0;
    }
            close(s);
    return 1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_ifdown(struct ppp *ppp)
{
    struct ifreq ifr;
    int 	s;

    s = socket(PF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return 0;
    
    sprintf(ifr.ifr_name, "%s%d", ppp->name, ppp->ifunit);
    if (ioctl(s, SIOCGIFFLAGS, (caddr_t) &ifr) < 0) {
	//error("ioctl (SIOCGIFFLAGS): %m");
            close(s);
	return 0;
    }
    ifr.ifr_flags &= ~IFF_UP;
    if (ioctl(s, SIOCSIFFLAGS, (caddr_t) &ifr) < 0) {
	//error("ioctl(SIOCSIFFLAGS): %m");
            close(s);
	return 0;
    }
            close(s);
    return 1;
}

/* -----------------------------------------------------------------------------
* add the default route and DNS, using configd cache mechanism.
----------------------------------------------------------------------------- */
int ppp_addroute(struct ppp *ppp, u_int32_t loc, u_int32_t dst, u_int32_t usermask,
                 u_int32_t dns1, u_int32_t dns2, u_char *hostname)
{
    char			hasdns = dns1 || dns2;
    int				i;
    struct in_addr		addr, dnsaddr[2];
    CFMutableArrayRef		array;
    CFStringRef			ifname = 0;
    CFMutableDictionaryRef	ipv4_dict = 0, dns_dict = 0;
    SCDHandleRef 		ipv4_data = 0, dns_data = 0;
    CFStringRef			ipv4_key = 0, dns_key = 0;
    SCDStatus			status;
    CFStringRef			str;

    // first set ip address for this interface
    setipv4address(ppp, loc, dst, getmask(loc, usermask));

    // then publish configd cache information
    dnsaddr[0].s_addr = dns1;
    dnsaddr[1].s_addr = dns2;
    ifname = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s%d"), 
				      ppp->name, ppp->ifunit);       

    /* create the IP cache key and dict */
    ipv4_key = SCDKeyCreateNetworkInterfaceEntity(kSCCacheDomainState,
						  ifname,
						  kSCEntNetIPv4);
    ipv4_dict = CFDictionaryCreateMutable(0, 0,
                                          &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks);
    if (hasdns) {
        /* create the DNS cache key and dict */
        dns_key = SCDKeyCreateNetworkInterfaceEntity(kSCCacheDomainState,
						     ifname,
						     kSCEntNetDNS);
        dns_dict = CFDictionaryCreateMutable(0, 0,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks);
    }

    /* set the serviceid array */
    array = CFArrayCreateMutable(0, 1, &kCFTypeArrayCallBacks);
    str = CFStringCreateWithFormat(0, 0, CFSTR("%d"), ppp->serviceID);
    CFArrayAppendValue(array, str);
    CFRelease(str);
    CFDictionarySetValue(ipv4_dict, kSCCachePropNetServiceIDs, array);
    CFRelease(array);

    /* set the ip address array */
    addr.s_addr = loc;
    array = CFArrayCreateMutable(0, 1, &kCFTypeArrayCallBacks);
    str = CFStringCreateWithFormat(0, 0, CFSTR(IP_FORMAT), IP_LIST(&addr));
    CFArrayAppendValue(array, str);
    CFRelease(str);
    CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Addresses, array);
    CFRelease(array);

    /* set the ip dest array */
    addr.s_addr = dst;
    array = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);
    str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), 
				   IP_LIST(&addr));
    CFArrayAppendValue(array, str);
    CFRelease(str);
    CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4DestAddresses, array);
    CFRelease(array);

    /* set the router */
    addr.s_addr = dst;
    str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&addr));
    CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Router, str);
    CFRelease(str);

    /* set the hostname */
    if (hostname) {
        str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), hostname);
        CFDictionarySetValue(ipv4_dict,  CFSTR("Hostname"), str);
        CFRelease(str);
    }
    

    if (hasdns) {
        /* set the DNS servers */
        array = CFArrayCreateMutable(NULL, 2, &kCFTypeArrayCallBacks);
        for (i = 0; i < 2; i++) {
            if (dnsaddr[i].s_addr) {
                str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), 
					       IP_LIST(&dnsaddr[i]));
                CFArrayAppendValue(array, str);
                CFRelease(str);
            }
        }
        CFDictionarySetValue(dns_dict, kSCPropNetDNSServerAddresses, array);
        CFRelease(array);
    }

    /* get system configuration cache handles */
    ipv4_data = SCDHandleInit();
    SCDHandleSetData(ipv4_data, ipv4_dict);
    CFRelease(ipv4_dict);

    if (hasdns) {
        dns_data = SCDHandleInit();
        SCDHandleSetData(dns_data, dns_dict);
        CFRelease(dns_dict);
    }

    /* update atomically to avoid needless notifications */
    SCDLock(gCfgCache);
    SCDRemove(gCfgCache, ipv4_key);

    status = SCDAdd(gCfgCache, ipv4_key, ipv4_data);
    if (status != SCD_OK)
        warn(ppp, "SCDAdd IP %s failed: %s\n", ifname, SCDError(status));

    if (hasdns) {
        SCDRemove(gCfgCache, dns_key);
        status = SCDAdd(gCfgCache, dns_key, dns_data);
        if (status != SCD_OK)
            warn(ppp, "SCDAdd DNS %s returned: %s\n", ifname, SCDError(status));
    }

    SCDUnlock(gCfgCache);
    SCDHandleRelease(ipv4_data);
    CFRelease(ipv4_key);
    if (hasdns) {
        SCDHandleRelease(dns_data);
        CFRelease(dns_key);
    }
    CFRelease(ifname);
    return 1;
}

/* -----------------------------------------------------------------------------
* remove the default route and DNS, using configd cache mechanism.
----------------------------------------------------------------------------- */
int ppp_delroute(struct ppp *ppp, u_int32_t loc, u_int32_t dst)
{
    CFStringRef		dns_key, ipv4_key, ifname;

    ifname = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s%d"), 
				      ppp->name, ppp->ifunit);       

    /* create IP cache key */
    ipv4_key = SCDKeyCreateNetworkInterfaceEntity(kSCCacheDomainState,
						  ifname,
						  kSCEntNetIPv4);
    /* create DNS cache key */
    dns_key = SCDKeyCreateNetworkInterfaceEntity(kSCCacheDomainState,
						 ifname,
						 kSCEntNetDNS);
    /* update atomically to avoid needless notifications */
    SCDLock(gCfgCache);
    SCDRemove(gCfgCache, ipv4_key);
    SCDRemove(gCfgCache, dns_key);
    SCDUnlock(gCfgCache);

    CFRelease(dns_key);
    CFRelease(ipv4_key);
    CFRelease(ifname);

    // clear now interface address
    clearipv4address(ppp, loc, dst);

    return 1;
}


/* -----------------------------------------------------------------------------
determine if the system has any route to a given IP address.
For demand mode to work properly, we have to ignore routes through our own interface
----------------------------------------------------------------------------- */
int have_route_to(u_int32_t addr)
{
    return -1;
}

/* -----------------------------------------------------------------------------
Config the interface IP addresses and netmask
----------------------------------------------------------------------------- */
int setipv4address(struct ppp * ppp, u_int32_t o, u_int32_t h, u_int32_t m)
{
    struct ifaliasreq 	ifra;
    //  struct ifreq 	ifr;
    int 		s;

    s = socket(PF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return 0;

    sprintf(ifra.ifra_name, "%s%d", ppp->name, ppp->ifunit);
    SET_SA_FAMILY(ifra.ifra_addr, AF_INET);
    ((struct sockaddr_in *) &ifra.ifra_addr)->sin_addr.s_addr = o;
    SET_SA_FAMILY(ifra.ifra_broadaddr, AF_INET);
    ((struct sockaddr_in *) &ifra.ifra_broadaddr)->sin_addr.s_addr = h;
    if (m != 0) {
        SET_SA_FAMILY(ifra.ifra_mask, AF_INET);
        ((struct sockaddr_in *) &ifra.ifra_mask)->sin_addr.s_addr = m;
    } else
        bzero(&ifra.ifra_mask, sizeof(ifra.ifra_mask));

#if 0
    BZERO(&ifr, sizeof(ifr));
    sprintf(ifra.ifra_name, "%s%d", ppp->name, ppp->ifunit);
    if (ioctl(s, SIOCDIFADDR, (caddr_t) &ifr) < 0) {
        if (errno != EADDRNOTAVAIL)
            ;   //warn("Couldn't remove interface address: %m");
    }
#endif

    /* this ioctl must use a PF_INET socket */
    if (ioctl(s, SIOCAIFADDR, (caddr_t) &ifra) < 0) {
        if (errno != EEXIST) {
            ;//error(ppp, "Couldn't set interface address (%s%d) error = %d : %m", ppp->name, ppp->unit, errno);
            close(s);
            return 0;
        }
        warn(ppp, "Couldn't set interface address: Address %I already exists", o);
    }
    close(s);
    return 1;
}

/* -----------------------------------------------------------------------------
Clear the interface IP addresses, and delete routes through the interface if possible
----------------------------------------------------------------------------- */
int clearipv4address(struct ppp *ppp, u_int32_t o, u_int32_t h)
{
    struct ifaliasreq ifra;
    int 		s;

    s = socket(PF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return 0;

    printf("ppp_clearipv4address loc = %d, dst = %d\n", o, h);

    sprintf(ifra.ifra_name, "%s%d", ppp->name, ppp->ifunit);
    SET_SA_FAMILY(ifra.ifra_addr, AF_INET);
    ((struct sockaddr_in *) &ifra.ifra_addr)->sin_addr.s_addr = o;
    SET_SA_FAMILY(ifra.ifra_broadaddr, AF_INET);
    ((struct sockaddr_in *) &ifra.ifra_broadaddr)->sin_addr.s_addr = h;
    bzero(&ifra.ifra_mask, sizeof(ifra.ifra_mask));

    /* this ioctl must use a PF_INET socket */
    if (ioctl(s, SIOCDIFADDR, (caddr_t) &ifra) < 0) {
        if (errno != EADDRNOTAVAIL)
            warn(ppp, "Couldn't delete interface address: %m");
        close(s);
        return 0;
    }
    close(s);
    return 1;
}

/* -----------------------------------------------------------------------------
Return user specified netmask, modified by any mask we might determine
for address `addr' (in network byte order).
Here we scan through the system's list of interfaces, looking for any
non-point-to-point interfaces which might appear to be on the same network
as `addr'.  If we find any, we OR in their netmask to the user-specified netmask
----------------------------------------------------------------------------- */
u_int32_t getmask(u_int32_t addr, u_int32_t usermask)
{
    u_int32_t 		mask, nmask, ina;
    struct ifreq 	*ifr, *ifend, ifreq;
    struct ifconf 	ifc;
    struct ifreq 	ifs[MAX_IFS];
    int 		s;

    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        ;//fatal("Couldn't create IP socket: %m");

        addr = ntohl(addr);
        if (IN_CLASSA(addr))	/* determine network mask for address class */
            nmask = IN_CLASSA_NET;
    else if (IN_CLASSB(addr))
        nmask = IN_CLASSB_NET;
    else
        nmask = IN_CLASSC_NET;
    /* class D nets are disallowed by bad_ip_adrs */
    mask = usermask | htonl(nmask);

    /*
     * Scan through the system's network interfaces.
     */
    ifc.ifc_len = sizeof(ifs);
    ifc.ifc_req = ifs;
    if (ioctl(s, SIOCGIFCONF, &ifc) < 0) {
        //warn(ppp, "ioctl(SIOCGIFCONF): %m");
        close(s);
        return mask;
    }
    ifend = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);
    for (ifr = ifc.ifc_req; ifr < ifend; ifr = (struct ifreq *)
         ((char *)&ifr->ifr_addr + ifr->ifr_addr.sa_len)) {
        /*
         * Check the interface's internet address.
         */
        if (ifr->ifr_addr.sa_family != AF_INET)
            continue;
        ina = ((struct sockaddr_in *) &ifr->ifr_addr)->sin_addr.s_addr;
        if ((ntohl(ina) & nmask) != (addr & nmask))
            continue;
        /*
         * Check that the interface is up, and not point-to-point or loopback.
         */
        strlcpy(ifreq.ifr_name, ifr->ifr_name, sizeof(ifreq.ifr_name));
        if (ioctl(s, SIOCGIFFLAGS, &ifreq) < 0)
            continue;
        if ((ifreq.ifr_flags & (IFF_UP|IFF_POINTOPOINT|IFF_LOOPBACK))
            != IFF_UP)
            continue;
        /*
         * Get its netmask and OR it into our mask.
         */
        if (ioctl(s, SIOCGIFNETMASK, &ifreq) < 0)
            continue;
        mask |= ((struct sockaddr_in *)&ifreq.ifr_addr)->sin_addr.s_addr;
    }

    close(s);
    return mask;
}

