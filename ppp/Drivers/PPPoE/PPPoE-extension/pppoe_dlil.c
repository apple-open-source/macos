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

#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>

#include <net/dlil.h>

/* include ppp domain as we use the ppp domain family */
#include "../../../Family/ppp_domain.h"

#include "PPPoE.h"
#include "pppoe_rfc.h"
#include "pppoe_dlil.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

struct pppoe_if {
    TAILQ_ENTRY(pppoe_if) next;
    u_short          	unit;
    u_short		refcnt;
    u_long	    	dl_tag;
};

/* -----------------------------------------------------------------------------
Declarations
----------------------------------------------------------------------------- */
int pppoe_dlil_input(struct mbuf *m, char *frame_header, struct ifnet *ifp,
                     u_long dl_tag, int sync_ok);
int pppoe_dlil_pre_output(struct ifnet *ifp, struct mbuf **m0, struct sockaddr *dst_netaddr,
                          caddr_t route, char *type, char *edst, u_long dl_tag );
int pppoe_dlil_event();
int pppoe_dlil_ioctl(u_long dl_tag, struct ifnet *ifp, u_long command, caddr_t data);
void pppoe_dlil_kern_event(struct socket* so, caddr_t ref, int waitf);
void pppoe_dlil_detaching(u_int16_t unit);


/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

TAILQ_HEAD(, pppoe_if) 	pppoe_if_head;
static struct socket	*pppoe_evt_so;
int	event_socket;

/* -----------------------------------------------------------------------------
intialize pppoe datalink attachment strutures
----------------------------------------------------------------------------- */
int pppoe_dlil_init()
{
    int 		err;
    struct kev_request 	kev;

    TAILQ_INIT(&pppoe_if_head);
    
    pppoe_evt_so = 0;
    
    /* Create a PF_SYSTEM socket so we can listen for events */
    err = socreate(PF_SYSTEM, &pppoe_evt_so, SOCK_RAW, SYSPROTO_EVENT);
    if (err || (pppoe_evt_so == NULL)) {
        /*
         * We will not get attaching or detaching events in this case.
         * We should probably prevent any sockets from binding so we won't
         * panic later if the interface goes away.
         */
        log(LOG_INFO, "pppoe_dlil_init: cannot create socket event, error = %d\n", err);
        return 0;	// still return OK
    }
    
    /* Install a callback function for the socket */
    pppoe_evt_so->so_rcv.sb_flags |= SB_NOTIFY|SB_UPCALL;
    pppoe_evt_so->so_upcall = pppoe_dlil_kern_event;
    pppoe_evt_so->so_upcallarg = NULL;
    
    /* Configure the socket to receive the events we're interested in */
    kev.vendor_code = KEV_VENDOR_APPLE;
    kev.kev_class = KEV_NETWORK_CLASS;
    kev.kev_subclass = KEV_DL_SUBCLASS;
    err = pppoe_evt_so->so_proto->pr_usrreqs->pru_control(pppoe_evt_so, SIOCSKEVFILT, (caddr_t)&kev, 0, 0);
    if (err) {
        /*
         * We will not get attaching or detaching events in this case.
         * We should probably prevent any sockets from binding so we won't
         * panic later if the interface goes away.
         */
        log(LOG_INFO, "pppoe_dlil_init: cannot set event filter, error = %d\n", err);
        return 0;	// still return OK
    }

    return 0;
}

/* -----------------------------------------------------------------------------
dispose pppoe datalink structures
can't dispose if clients are still attached
----------------------------------------------------------------------------- */
int pppoe_dlil_dispose()
{
    if (!TAILQ_EMPTY(&pppoe_if_head))
        return 1;
        
    if (pppoe_evt_so) {
        soclose(pppoe_evt_so);
        pppoe_evt_so = 0;
    }
    return 0;
}

/* -----------------------------------------------------------------------------
attach pppoe to ethernet unit
returns 0 and the dl_tag when successfully attached
----------------------------------------------------------------------------- */
int pppoe_dlil_attach(u_short unit, u_long *dl_tag)
{
    struct pppoe_if  		*pppoeif;
    int				ret;
    struct dlil_proto_reg_str   reg;
    struct dlil_demux_desc      desc, desc2;
    u_int16_t			ctrl_protocol = PPPOE_ETHERTYPE_CTRL;
    u_int16_t			data_protocol = PPPOE_ETHERTYPE_DATA;
    
    TAILQ_FOREACH(pppoeif, &pppoe_if_head, next) {
        if (pppoeif->unit == unit) {
            *dl_tag = pppoeif->dl_tag;
            pppoeif->refcnt++;
            return 0;
        }
    }

    MALLOC(pppoeif, struct pppoe_if *, sizeof(struct pppoe_if), M_TEMP, M_WAITOK);
    if (!pppoeif) {
        log(LOG_INFO, "pppoe_dlil_attach : Can't allocate attachment structure\n");
        return 1;
    }

    bzero(&reg, sizeof(struct dlil_proto_reg_str));
    
    // define demux for PPPoE
    TAILQ_INIT(&reg.demux_desc_head);
    bzero(&desc, sizeof(struct dlil_demux_desc));
    desc.type = DLIL_DESC_RAW;
    desc.native_type = (char *) &ctrl_protocol;
    TAILQ_INSERT_TAIL(&reg.demux_desc_head, &desc, next);
    desc2 = desc;
    desc2.native_type = (char *) &data_protocol;
    TAILQ_INSERT_TAIL(&reg.demux_desc_head, &desc2, next);
    
    reg.interface_family = APPLE_IF_FAM_ETHERNET;
    reg.protocol_family  = PF_PPP;
    reg.unit_number      = unit;
    reg.input            = pppoe_dlil_input;
    reg.pre_output       = pppoe_dlil_pre_output;
    reg.event            = pppoe_dlil_event;
    reg.ioctl            = pppoe_dlil_ioctl;

    ret = dlil_attach_protocol(&reg, dl_tag);
    if (ret) {
        log(LOG_INFO, "pppoe_dlil_attach: error = 0 x%x\n", ret);
        FREE(pppoeif, M_TEMP);
        return ret;
    }

    bzero(pppoeif, sizeof(struct pppoe_if));

    pppoeif->unit = unit;
    pppoeif->dl_tag = *dl_tag;
    pppoeif->refcnt = 1;

    TAILQ_INSERT_TAIL(&pppoe_if_head, pppoeif, next);
    return 0;
}

/* -----------------------------------------------------------------------------
detach pppoe from ethernet unit
----------------------------------------------------------------------------- */
int pppoe_dlil_detach(u_long dl_tag)
{
    struct pppoe_if  	*pppoeif;
    
    if (dl_tag == 0)
        return 0;

    TAILQ_FOREACH(pppoeif, &pppoe_if_head, next) {
        if (pppoeif->dl_tag == dl_tag) {
            pppoeif->refcnt--;
            if (pppoeif->refcnt == 0) {
                dlil_detach_protocol(pppoeif->dl_tag);
                TAILQ_REMOVE(&pppoe_if_head, pppoeif, next);
                FREE(pppoeif, M_TEMP);
            }
            break;
        }
    }

    return 0;
}

/* -----------------------------------------------------------------------------
upcall function for kernel events
----------------------------------------------------------------------------- */
void pppoe_dlil_kern_event(struct socket* so, caddr_t ref, int waitf)
{
    struct mbuf 		*m = NULL;
    struct kern_event_msg 	*msg;
    struct uio 			auio = { 0 };
    int 			err, flags;
    struct net_event_data 	*ev_data;
    
    // Get the data
    auio.uio_resid = 1000000; // large number to get all of the data
    flags = MSG_DONTWAIT;
    err = soreceive(so, 0, &auio, &m, 0, &flags);
    if (err || (m == NULL))
        return;
    
    // cast the mbuf to a kern_event_msg
    // this is dangerous, doesn't handle linked mbufs
    msg = mtod(m, struct kern_event_msg *);
    
    // check for detaching, assume even filtering is working
    if (msg->event_code == KEV_DL_IF_DETACHING) {
        
        ev_data = (struct net_event_data*)msg->event_data;

        if (ev_data->if_family == APPLE_IF_FAM_ETHERNET)
            pppoe_dlil_detaching(ev_data->if_unit);
    }
    
    m_free(m);
}

/* -----------------------------------------------------------------------------
ethernet unit wants to detach
----------------------------------------------------------------------------- */
void pppoe_dlil_detaching(u_int16_t unit)
{
    struct pppoe_if  	*pppoeif;

    TAILQ_FOREACH(pppoeif, &pppoe_if_head, next) {
        if (pppoeif->unit == unit) {
            pppoe_rfc_lower_detaching(pppoeif->dl_tag);
            break;
        }
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int pppoe_dlil_pre_output(struct ifnet *ifp, struct mbuf **m0, struct sockaddr *dst_netaddr,
                          caddr_t route, char *type, char *edst, u_long dl_tag )
{

    struct ether_header 	*eh;

    //log(LOGVAL, "pppenet_pre_output, ifp = 0x%x, dl_tag = 0x%x\n", ifp, dl_tag);

    // fill in expected type and dst from previously set dst_netaddr
    // looks complicated to me
    eh = (struct ether_header *)dst_netaddr->sa_data;


    memcpy(edst, eh->ether_dhost, sizeof(eh->ether_dhost));
    *(u_int16_t *)type = eh->ether_type;

    //log(LOGVAL, "addr = 0x%x%x%x%x%x%x, type = 0x%lx\n", edst[0], edst[1],
    //    edst[2], edst[3], edst[4], edst[5], *type);

    return 0;
}

/* -----------------------------------------------------------------------------
 should handle network up/down...
----------------------------------------------------------------------------- */
int  pppoe_dlil_event()
{

    log(LOGVAL, "pppoe_dlil_event\n");
    return 0;
}

/* -----------------------------------------------------------------------------
pppoe specific ioctl, nothing defined
----------------------------------------------------------------------------- */
int pppoe_dlil_ioctl(u_long dl_tag, struct ifnet *ifp, u_long command, caddr_t data)
{
    
    log(LOGVAL, "pppoe_dlil_ioctl, dl_tag = 0x%x\n", dl_tag);
    return 0;
}


/* -----------------------------------------------------------------------------
* Process a received pppoe packet;
* the packet is in the mbuf chain m without
* the ether header, which is provided separately.
----------------------------------------------------------------------------- */
int pppoe_dlil_input(struct mbuf *m, char *frame_header, struct ifnet *ifp,
                     u_long dl_tag, int sync_ok)
{

    //unsigned char *p = frame_header;
    //unsigned char *data;
    struct ether_header *eh = (struct ether_header *)frame_header;

    //log(LOGVAL, "pppenet_input, dl_tag = 0x%x\n", dl_tag);
    //log(LOGVAL, "pppenet_input: enet_header dst : %x:%x:%x:%x:%x:%x - src %x:%x:%x:%x:%x:%x - type - %4x\n",
        //p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11],ntohs(*(u_int16_t *)&p[12]));


    //data = mtod(m, unsigned char *);
    //log(LOGVAL, "pppenet_input: data 0x %x %x %x %x %x %x\n",
    //    data[0],data[1],data[2],data[3],data[4],data[5]);

    // only the dl_tag discriminate client at this point
    // pppoe will have to look at the session id to select the appropriate socket

    pppoe_rfc_lower_input(dl_tag, m, frame_header + ETHER_ADDR_LEN, ntohs(eh->ether_type));

    return 0;
}

/* -----------------------------------------------------------------------------
called from pppenet_proto when data need to be sent
----------------------------------------------------------------------------- */
int pppoe_dlil_output(u_long dl_tag, struct mbuf *m, u_int8_t *to, u_int16_t typ)
{
    struct ether_header 	*eh;
    struct sockaddr 		sa;

    eh = (struct ether_header *)sa.sa_data;
    (void)memcpy(eh->ether_dhost, to, sizeof(eh->ether_dhost));
    eh->ether_type = htons(typ);
    sa.sa_family = AF_UNSPEC;
    sa.sa_len = sizeof(sa);

    dlil_output(dl_tag, m, 0, &sa, 0);
    return 0;
}

