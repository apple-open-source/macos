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
*
*  History :
*
*  Aug 2000 - 	Christophe Allie - created.
*
*  Theory of operation :
*
*  this file implements the pppoe driver for the ppp family
*
*  it's the responsability of the driver to update the statistics
*  whenever that makes sense
*     ifnet.if_lastchange = a packet is present, a packet starts to be sent
*     ifnet.if_ibytes = nb of correct PPP bytes received (does not include escapes...)
*     ifnet.if_obytes = nb of correct PPP bytes sent (does not include escapes...)
*     ifnet.if_ipackets = nb of PPP packet received
*     ifnet.if_opackets = nb of PPP packet sent
*     ifnet.if_ierrors = nb on input packets in error
*     ifnet.if_oerrors = nb on ouptut packets in error
*
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Includes
----------------------------------------------------------------------------- */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/sockio.h>

#include <machine/spl.h>

#include <net/if_types.h>
#include <net/dlil.h>

#include "../../../Family/PPP.kmodproj/ppp.h"

#include "pppoe_rfc.h"
#include "pppoe_wan.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

#define DEF_PPPOE_WAN 1

struct pppoe_wan {
    /* first, the ifnet structure... */
    struct ifnet 	lk_if;			/* network-visible interface */

    /* administrative info */
    TAILQ_ENTRY(pppoe_wan) next;
    void 		*rfc;			/* pppoe protocol structure */

    /* settings */
    
    /* output data */

    /* input data */

    /* log purpose */
};

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

static int 	pppoe_wan_attach(u_short unit, u_short ether_unit);
static int 	pppoe_wan_detach(u_short unit);
//static int 	pppoe_wan_adjust(u_long count);
static int 	pppoe_wan_input(void *data, struct mbuf *m);
static int	pppoe_wan_output(struct ifnet *ifp, struct mbuf *m);
static void 	pppoe_wan_event(void *data, u_int32_t event, u_int32_t msg);
static int	pppoe_wan_if_free(struct ifnet *ifp);
static int 	pppoe_wan_ioctl(struct ifnet *ifp, u_long cmd, void *data);
static int 	pppoe_wan_sendevent(struct ifnet *ifp, u_long code);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

static TAILQ_HEAD(, pppoe_wan) 	pppoe_wan_head;
static int pppoe_wan_firstinuse = 0;


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int pppoe_wan_init()
{
    u_short 		i;

    TAILQ_INIT(&pppoe_wan_head);

    // attachment/detachment should be more dynamic, via ioctls...
    for (i = 0; i < DEF_PPPOE_WAN; i++) {

        // we always attach to en0... should be selectable
        pppoe_wan_attach(i, 0);
    }

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int pppoe_wan_dispose()
{
    struct pppoe_wan  	*wan;

    log(LOG_INFO, "pppoe_wan_dispose\n");

    while (wan = TAILQ_FIRST(&pppoe_wan_head))
        pppoe_wan_detach(wan->lk_if.if_unit);

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
#if 0
int pppoe_wan_adjust(u_long count)
{
    struct pppoe_wan  	*wan;
    u_long		nb = 0;

    log(LOG_INFO, "pppoe_wan_adjust, number of interfaces requested = %d\n", count);

    TAILQ_FOREACH(wan, &pppoe_wan_head, next) {
        nb++;
    }

    if (nb == count) {
        // nothing to do...
        return 0;
    }

    if (nb < count) {
        // add some more...
        while (nb < count) {
            // should take care of conflict with other ifnumbers
            pppoe_wan_attach(nb, 0);
            nb++;
        }
        return 0;
    }

    // remove some
    while (nb > count) {
        // should take care of conflict with other ifnumbers
        nb --;
        // pb to adjust is inferface is in use for a networking protocol
        TAILQ_FOREACH(wan, &pppoe_wan_head, next) {
            if (wan->lk_if.if_unit == nb) {
                if (wan->lk_if.if_flags & IFF_PPP_CONNECTED)
                //if (wan->lk_if.if_flags & IFF_UP)
                    return 1;
           }
        }
        pppoe_wan_detach(nb);
    }

    return 0;
}
#endif
/* -----------------------------------------------------------------------------
detach pppoe interface dlil layer
----------------------------------------------------------------------------- */
int pppoe_wan_attach(u_short unit, u_short ether_unit)
{
    int 		ret;	
    struct pppoe_wan  	*wan;

    // then, alloc the structure...
    MALLOC(wan, struct pppoe_wan *, sizeof(struct pppoe_wan), M_DEVBUF, M_WAITOK);
    if (!wan)
        return ENOMEM;

    bzero(wan, sizeof(struct pppoe_wan));

    // it's time now to register our brand new channel
    wan->lk_if.if_name 		= APPLE_PPP_NAME_PPPoE;
    wan->lk_if.if_family 	= (((u_long) APPLE_IF_FAM_PPP_PPPoE) << 16) + APPLE_IF_FAM_PPP;
    wan->lk_if.if_mtu 		= PPPOE_MTU;
    wan->lk_if.if_flags 	= IFF_POINTOPOINT | IFF_MULTICAST | IFF_DEBUG;
    wan->lk_if.if_type 		= IFT_PPP;
    wan->lk_if.if_physical 	= PPP_PHYS_PPPoE;
    wan->lk_if.if_hdrlen 	= PPP_HDRLEN;
    wan->lk_if.if_ioctl 	= pppoe_wan_ioctl;
    wan->lk_if.if_output 	= pppoe_wan_output;
    wan->lk_if.if_free 		= pppoe_wan_if_free;
    wan->lk_if.if_baudrate 	= 10000000; // 10 Mbits/s ???
    wan->lk_if.if_eflags 	= IFF_PPP_DEL_AC;
    getmicrotime(&wan->lk_if.if_lastchange);

    wan->lk_if.if_unit = unit++;
    ret = dlil_if_attach(&wan->lk_if);

    // register this interface to the pppoe protocol
    if (pppoe_rfc_new_client(wan, ether_unit, &wan->rfc, pppoe_wan_input, pppoe_wan_event)) {
        dlil_if_detach(&wan->lk_if);
        FREE(wan, M_DEVBUF);
        return ENOMEM;
    }

    TAILQ_INSERT_TAIL(&pppoe_wan_head, wan, next);

    return 0;
}

/* -----------------------------------------------------------------------------
detach pppoe interface dlil layer
----------------------------------------------------------------------------- */
int pppoe_wan_detach(u_short unit)
{
    int 		ret;
    struct pppoe_wan  	*wan;

    TAILQ_FOREACH(wan, &pppoe_wan_head, next) {
        if (wan->lk_if.if_unit == unit) {
            /* disconnect first, just in case... */
            pppoe_rfc_disconnect(wan->rfc);
            pppoe_rfc_free_client(wan->rfc);
            ret = dlil_if_detach(&wan->lk_if);
            switch (ret) {
                case 0:
                    break;
                case DLIL_WAIT_FOR_FREE:
                    sleep(&wan->lk_if, PZERO+1);
                    break;
                default:
                    return KERN_FAILURE;
            }

            TAILQ_REMOVE(&pppoe_wan_head, wan, next);
            FREE(wan, M_DEVBUF);
            break;
        }
    }

    return 0;
}

/* -----------------------------------------------------------------------------
find a free unit in the interface list
----------------------------------------------------------------------------- */
int pppoe_wan_findfreeunit(u_short *freeunit)
{
    struct pppoe_wan  	*wan;
    u_short 		unit = 0;

    TAILQ_FOREACH(wan, &pppoe_wan_head, next) {
        if (wan->lk_if.if_unit == unit) {
            wan = TAILQ_FIRST(&pppoe_wan_head); // reloop
            unit++;
        }
    }
    *freeunit = unit;
    return 0;
}

/* -----------------------------------------------------------------------------
called from pppoe_rfc when change state occurs
----------------------------------------------------------------------------- */
void pppoe_wan_event(void *data, u_int32_t event, u_int32_t msg)
{
    int 		s = splnet();
    struct pppoe_wan 	*wan = (struct pppoe_wan *)data;

    switch (event) {
        case PPPOE_EVT_RINGING:
            wan->lk_if.if_flags 	&= ~IFF_PPP_CONNECTED;
            //wan->lk_if.if_flags 	|= IFF_RUNNING;
            //wan->lk_if.if_flags 	&= ~IFF_UP;
            wan->lk_if.if_flags 	|= IFF_PPP_INCOMING_CALL;
            wan->lk_if.if_flags 	&= ~(IFF_PPP_CONNECTING | IFF_PPP_DISCONNECTING);
            pppoe_wan_sendevent(&wan->lk_if, KEV_PPP_RINGING);
            break;

        case PPPOE_EVT_CONNECTED:
            //wan->lk_if.if_flags 	|= IFF_RUNNING | IFF_UP;
            wan->lk_if.if_flags 	|= IFF_PPP_CONNECTED;
            wan->lk_if.if_flags 	&= ~(IFF_PPP_INCOMING_CALL | IFF_PPP_CONNECTING | IFF_PPP_DISCONNECTING);
            pppoe_wan_sendevent(&wan->lk_if, KEV_PPP_CONNECTED);
            break;
            
        case PPPOE_EVT_DISCONNECTED:
            wan->lk_if.if_ipackets = 0;
            wan->lk_if.if_opackets = 0;
            wan->lk_if.if_ibytes = 0;
            wan->lk_if.if_obytes = 0;
            wan->lk_if.if_ierrors = 0;
            wan->lk_if.if_oerrors = 0;
            wan->lk_if.if_flags 	&= ~IFF_PPP_CONNECTED;
            //wan->lk_if.if_flags 	&= ~(IFF_RUNNING | IFF_UP);
            wan->lk_if.if_flags 	&= ~(IFF_PPP_INCOMING_CALL | IFF_PPP_CONNECTING | IFF_PPP_DISCONNECTING);
            pppoe_wan_sendevent(&wan->lk_if, KEV_PPP_DISCONNECTED);
            break;
    }
    splx(s);
}

/* -----------------------------------------------------------------------------
called from pppoe_rfc when data are present
----------------------------------------------------------------------------- */
int pppoe_wan_input(void *data, struct mbuf *m)
{
    struct pppoe_wan 	*wan = (struct pppoe_wan *)data;
    
    wan->lk_if.if_ipackets++;
    wan->lk_if.if_ibytes += m->m_pkthdr.len;
    getmicrotime(&wan->lk_if.if_lastchange);

    //log(LOG_INFO, "pppoe_wan_input\n");
    m->m_pkthdr.rcvif = &wan->lk_if;
    // ppp packets are passed raw, header will be reconstructed in the packet
    m->m_pkthdr.header = 0;
    dlil_input(&wan->lk_if, m, m);

    return 0;
}

/* -----------------------------------------------------------------------------
This gets called when the interface is freed
(if dlil_if_detach has returned DLIL_WAIT_FOR_FREE)
----------------------------------------------------------------------------- */
int pppoe_wan_if_free(struct ifnet *ifp)
{
    wakeup(ifp);
    return 0;
}

/* -----------------------------------------------------------------------------
Process an ioctl request to the ppp interface
----------------------------------------------------------------------------- */
int pppoe_wan_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
    struct pppoe_wan 	*wan = (struct pppoe_wan *)ifp;//, *wan2;
    struct ifreq 	*ifr = (struct ifreq *)data;
    struct ifpppreq 	*ifpppr = (struct ifpppreq *)data;
    int 		error = 0, len, i;
    char 		name[256], service[256];
    u_short		unit;

    //LOGDBG(ifp, (LOGVAL, "pppoe_wan_ioctl, cmd = 0x%x\n", cmd));

    switch (cmd) {

        case SIOCSIFPPP:
            switch (ifpppr->ifr_code) {
                case IFPPP_CONNECT:
                    //log(LOG_INFO, "pppoe_wan_ioctl IFPPP_CONNECT, service = %s\n", ifpppr->ifr_connect);
                    len = strlen(ifpppr->ifr_connect);
                    for (i = 0; i < len; i++) {
                        if (ifpppr->ifr_connect[i] == '\\')
                            break;
                        service[i] = ifpppr->ifr_connect[i];
                    }
                    service[i] = 0;
                    name[0] = 0;
                    if (i < len)
                        strncpy(name, &ifpppr->ifr_connect[i + 1], sizeof(name));
                    //strcpy(service, ifpppr->ifr_connect);
                    //strcpy(name, &ifpppr->ifr_connect[strlen(service) + 1]);
                   if (pppoe_rfc_connect(wan->rfc, name, service))
                        error = EINVAL;		/* XXX ??? */
                    else {
                        wan->lk_if.if_flags |= IFF_RUNNING;
                        getmicrotime(&wan->lk_if.if_lastchange);

                        log(LOG_INFO, "pppoe_wan_ioctl send event\n");
                        pppoe_wan_sendevent(&wan->lk_if, KEV_PPP_CONNECTING);

                    }
                    return error;
                   break;
                case IFPPP_DISCONNECT:
                    pppoe_rfc_disconnect(wan->rfc);
                  break;
                case IFPPP_ABORT:
                    if (pppoe_rfc_abort(wan->rfc))
                        error = EINVAL;		/* XXX ??? */
                  break;
                case IFPPP_LISTEN:
                    strcpy(service, ifpppr->ifr_listen);
                    strcpy(name, &ifpppr->ifr_listen[strlen(service) + 1]);
                    if (pppoe_rfc_bind(wan->rfc, name, service))
                        error = EINVAL;		/* XXX ??? */
                    else {
                        if (pppoe_rfc_listen(wan->rfc))
                            error = EINVAL;		/* XXX ??? */
                        else
                            pppoe_wan_sendevent(&wan->lk_if, KEV_PPP_LISTENING);
                    }
                 break;
                case IFPPP_ACCEPT:
                    if (pppoe_rfc_accept(wan->rfc))
                        error = EINVAL;		/* XXX ??? */
                  break;
                case IFPPP_REFUSE:
                    if (pppoe_rfc_abort(wan->rfc))
                        error = EINVAL;		/* XXX ??? */
                   break;
                case IFPPP_LOOPBACK:
                    pppoe_rfc_command(wan->rfc, PPPOE_CMD_SETLOOPBACK, &ifpppr->ifr_loopback);
                    break;
#if 0
                case IFPPP_NBLINKS: // humm... if we go down to 0 interfaces, then we can't reincrease the number again...
                    if (ifpppr->ifr_nblinks)
                        pppoe_wan_adjust(ifpppr->ifr_nblinks);
                    break;
#endif
                case IFPPP_DEVICE: 
                    // need to change attachment unit
                    break;
                case IFPPP_NEWIF:
                    if (pppoe_wan_firstinuse) {
                        pppoe_wan_findfreeunit(&unit);
                        error = pppoe_wan_attach(unit, 0);
                        ifpppr->ifr_newif = 0x10000 + unit; // really new
                    }
                    else {
                        ifpppr->ifr_newif = 0; 
                        pppoe_wan_firstinuse = 1;
                    }
 
                   break;
                case IFPPP_DISPOSEIF:
                    if (ifpppr->ifr_disposeif) 
                        error = pppoe_wan_detach(ifpppr->ifr_disposeif);
                    else 
                        pppoe_wan_firstinuse = 0;
                   break;
                default:
                    error = EOPNOTSUPP;
            }
            break;

        case SIOCGIFPPP:
            switch (ifpppr->ifr_code) {
                case IFPPP_CAPS:
                    bzero(&ifpppr->ifr_caps, sizeof(ifpppr->ifr_caps));
                    strcpy(ifpppr->ifr_caps.link_name, "PPPoE");
                    ifpppr->ifr_caps.physical = PPP_PHYS_PPPoE;
                    ifpppr->ifr_caps.flags = PPP_CAPS_DIAL + /*PPP_CAPS_DYNLINK + */PPP_CAPS_LOOPBACK;
                    ifpppr->ifr_caps.max_mtu = PPPOE_MTU;
                    ifpppr->ifr_caps.max_mru = PPPOE_MTU;
                    //ifpppr->ifr_caps.cur_links = 0; 
                    //TAILQ_FOREACH(wan2, &pppoe_wan_head, next) {
                    //    ifpppr->ifr_caps.cur_links++;
                    //}
                    //ifpppr->ifr_caps.max_links = 32; // ???
                    break;
#if 0
                case IFPPP_NBLINKS:
                    ifpppr->ifr_nblinks = 0;
                    TAILQ_FOREACH(wan2, &pppoe_wan_head, next) {
                        ifpppr->ifr_nblinks++;
                    }
                    break;
#endif
                case IFPPP_LOOPBACK:
                    pppoe_rfc_command(wan->rfc, PPPOE_CMD_GETLOOPBACK, &ifpppr->ifr_loopback);
                    break;
                 case IFPPP_DEVICE: 
                    // need to return attachment unit
                    break;
                default:
                    error = EOPNOTSUPP;
           }
            break;
            
        case SIOCSIFMTU:
            if (ifr->ifr_mtu > PPPOE_MTU) {
                log(LOG_INFO, "PPPoE Wan : SIOCSIFMTU  asked = %d, max = %d\n", ifr->ifr_mtu, PPPOE_MTU);
                error = EINVAL;
           }
            else
                ifp->if_mtu = ifr->ifr_mtu;
            break;

        default:
            error = EOPNOTSUPP;
    }
    return error;
}

/* -----------------------------------------------------------------------------
This gets called at splnet from if_ppp.c at various times
when there is data ready to be sent
----------------------------------------------------------------------------- */
int pppoe_wan_output(struct ifnet *ifp, struct mbuf *m)
{
    struct pppoe_wan 	*wan = (struct pppoe_wan *)ifp;
    int 		s;

    //if ((ifp->if_flags & IFF_RUNNING) == 0
    if ((ifp->if_flags & IFF_PPP_CONNECTED) == 0) {
//        || (ifp->if_flags & IFF_UP) == 0) {

        return ENETDOWN;
    }

    getmicrotime(&wan->lk_if.if_lastchange);

    s = splnet();
    if (pppoe_rfc_output(wan->rfc, m)) {
        wan->lk_if.if_oerrors++;
        splx(s);
        return ENOBUFS;
    }
    splx(s);

    wan->lk_if.if_opackets++;
    wan->lk_if.if_obytes += m->m_pkthdr.len;

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int pppoe_wan_sendevent(struct ifnet *ifp, u_long code)
{
    struct kev_ppp_msg		evt;

    bzero(&evt, sizeof(struct kev_ppp_msg));
    evt.total_size 	= KEV_MSG_HEADER_SIZE + sizeof(struct kev_ppp_data);
    evt.vendor_code 	= KEV_VENDOR_APPLE;
    evt.kev_class 	= KEV_NETWORK_CLASS;
    evt.kev_subclass 	= KEV_PPP_SUBCLASS;
    evt.id 		= 0;
    evt.event_code 	= code;
    evt.event_data.link_data.if_family = ifp->if_family;
    evt.event_data.link_data.if_unit = ifp->if_unit;
    bcopy(ifp->if_name, &evt.event_data.link_data.if_name[0], IFNAMSIZ);    

    dlil_event(ifp, (struct kern_event_msg *)&evt);
    return 0;
}
